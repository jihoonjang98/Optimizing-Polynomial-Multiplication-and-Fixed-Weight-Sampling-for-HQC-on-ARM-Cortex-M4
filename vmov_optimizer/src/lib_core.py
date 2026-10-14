#!/usr/bin/env python3
import math
import os
import random
import re

GPR = ['r0', 'r1', 'r2', 'r3',  'r4',  'r5',  'r6',
       'r7', 'r8', 'r9', 'r10', 'r11', 'r12', 'r14']

# Eviction victim = argmax(nextuse - SPILL_PENALTY*is_dirty), clean preferred on
# ties. Env-configurable so it survives multiprocessing spawn workers.
SPILL_PENALTY = int(os.environ.get("SPILL_PENALTY", "1"))


def parse_function(body_text):
    lines = body_text.splitlines()
    eor_pat = re.compile(
        r'eor\s+\w+\s*,\s*\w+\s*,\s*\w+\s*@\s*x\[(\d+)\]\s*=\s*x\[\d+\]\s*\^\s*x\[(\d+)\]')
    str_pat = re.compile(
        r'(?:str|vstr\.32)\s+\S+\s*,\s*\[r0,\s*#(\d+)\]\s*@\s*y\[(\d+)\]\s*=\s*x\[(\d+)\]')
    ops, stores = [], []
    for line in lines:
        m = eor_pat.search(line)
        if m:
            ops.append((int(m.group(1)), int(m.group(2))))
            continue
        m = str_pat.search(line)
        if m:
            stores.append((int(m.group(1)), int(m.group(3))))
    return ops, stores


def emit_count(ops, stores):
    """VMOV count for an order; must match lib_regen.emit. `stores` unused."""
    n = len(ops)
    value_count = max((max(d, s) for d, s in ops), default=-1) + 1
    next_pos = [float('inf')] * value_count
    next_use_after = [None] * n
    for i in range(n - 1, -1, -1):
        next_use_after[i] = tuple(next_pos)
        d, s = ops[i]
        next_pos[d] = i
        next_pos[s] = i

    reg_of = {}
    dirty = set()
    free = list(range(len(GPR) - 1, -1, -1))
    vmov = 0

    def alloc(v, i, keep=()):
        nonlocal vmov
        reg = reg_of.get(v)
        if reg is not None:
            return reg
        if free:
            reg = free.pop()
        else:
            victim, best = None, None
            nua = next_use_after[i]
            for x, r in reg_of.items():
                if x in keep:
                    continue
                key = (nua[x] - (SPILL_PENALTY if x in dirty else 0),
                       0 if x in dirty else 1)
                if best is None or key > best:
                    best, victim = key, x
            reg = reg_of.pop(victim)
            if victim in dirty:
                dirty.discard(victim)
                vmov += 1
        reg_of[v] = reg
        vmov += 1
        return reg

    for i, (dst, src) in enumerate(ops):
        alloc(dst, i, keep=(dst, src))
        alloc(src, i, keep=(dst, src))
        dirty.add(dst)

    for v, reg in reg_of.items():
        if reg == 0 and v in dirty:
            vmov += 1
    return vmov


def dependent(a, b):
    """True iff op a must stay before op b (they do not commute)."""
    i, j = a
    k, l = b
    if i != k and i != l and j != k and j != l:
        return False
    if j == l and i != k and i != l and j != k:
        return False
    if i == k and j != l and j != k and l != i:
        return False
    return True


def _dependency_graph(ops):
    n = len(ops)
    preds = [set() for _ in range(n)]
    succs = [set() for _ in range(n)]
    for j in range(n):
        for i in range(j):
            if dependent(ops[i], ops[j]):
                preds[j].add(i)
                succs[i].add(j)
    return preds, succs


def greedy_start(ops, reg_capacity=14):
    preds, succs = _dependency_graph(ops)
    n = len(ops)
    indeg = [len(preds[i]) for i in range(n)]
    ready = [i for i in range(n) if indeg[i] == 0]
    output = []
    recent = []

    def touch(x):
        if x in recent:
            recent.remove(x)
        recent.insert(0, x)
        if len(recent) > reg_capacity:
            recent.pop()

    def score(idx):
        d, s = ops[idx]
        cost = (d not in recent) + (s != d and s not in recent)
        bonus = 1 if recent and recent[0] in (d, s) else 0
        return cost - 0.5 * bonus

    while ready:
        ready.sort(key=lambda i: (score(i), i))
        nxt = ready.pop(0)
        output.append(nxt)
        d, s = ops[nxt]
        touch(d)
        touch(s)
        for k in succs[nxt]:
            indeg[k] -= 1
            if indeg[k] == 0:
                ready.append(k)
    if len(output) != n:
        raise RuntimeError("scheduling incomplete")
    return [ops[i] for i in output]


def random_start(ops, rng):
    preds, succs = _dependency_graph(ops)
    n = len(ops)
    indeg = [len(preds[i]) for i in range(n)]
    ready = [i for i in range(n) if indeg[i] == 0]
    output = []
    while ready:
        k = rng.randrange(len(ready))
        ready[k], ready[-1] = ready[-1], ready[k]
        nxt = ready.pop()
        output.append(nxt)
        for s in succs[nxt]:
            indeg[s] -= 1
            if indeg[s] == 0:
                ready.append(s)
    if len(output) != n:
        raise RuntimeError("scheduling incomplete")
    return [ops[i] for i in output]


def try_insertion(cur, i, j):
    op = cur[i]
    if j > i:
        for k in range(i + 1, j + 1):
            if dependent(op, cur[k]):
                return None
    elif j < i:
        for k in range(j, i):
            if dependent(cur[k], op):
                return None
    else:
        return None
    new = list(cur)
    new.insert(j, new.pop(i))
    return new


def anneal(ops, stores, iters=40000, seed=None, t0=1.2, t1=0.02, max_slide=8):
    """Simulated annealing over legal reorderings; objective is emit_count."""
    if iters < 2:
        raise ValueError("iters must be at least 2")
    if t0 <= 0 or t1 <= 0:
        raise ValueError("t0 and t1 must be positive")
    if max_slide < 1:
        raise ValueError("max_slide must be positive")

    if len(ops) < 2:
        order = list(ops)
        return order, emit_count(order, stores)

    rng = random.Random(seed)
    cur = list(ops)
    cur_v = emit_count(cur, stores)
    best = list(cur)
    best_v = cur_v
    n = len(cur)
    cool = t1 / t0

    for it in range(iters):
        if rng.random() < 0.5:
            i = rng.randrange(n - 1)
            j = i + 1
        else:
            i = rng.randrange(n)
            offset = rng.randint(-max_slide, max_slide)
            j = min(max(i + offset, 0), n - 1)

        cand = try_insertion(cur, i, j)
        if cand is None:
            continue

        cand_v = emit_count(cand, stores)
        delta = cand_v - cur_v
        temperature = t0 * cool ** (it / (iters - 1))

        if delta <= 0 or rng.random() < math.exp(-delta / temperature):
            cur = cand
            cur_v = cand_v
            if cand_v < best_v:
                best = list(cand)
                best_v = cand_v

    return best, best_v


FN_PAT = re.compile(r'(gft_mul_v(\d+):\n.*?)(?=\.size\s+gft_mul_v|\Z)', re.DOTALL)


def iter_functions(asm_text):
    pos = 0
    while True:
        m = FN_PAT.search(asm_text, pos)
        if not m:
            return
        yield int(m.group(2)), m.group(1)
        pos = m.end()
