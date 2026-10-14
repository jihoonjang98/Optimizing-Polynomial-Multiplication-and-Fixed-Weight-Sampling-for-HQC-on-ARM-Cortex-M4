#!/usr/bin/env python3
"""Asm regeneration and symbolic verification against the baseline store map."""
import re
from bisect import bisect_right
from collections import defaultdict

from lib_core import GPR, SPILL_PENALTY


def emit(ops, stores):
    gpr_of = {}
    used = set()
    dirty = {i: False for i in range(32)}
    out = []
    vmov = 0
    use_after = defaultdict(list)
    for j, (d, s) in enumerate(ops):
        use_after[d].append(j)
        use_after[s].append(j)

    def next_use(v, after_i):
        lst = use_after[v]
        k = bisect_right(lst, after_i)
        return lst[k] if k < len(lst) else float('inf')

    pool = GPR

    def alloc(v, i, reserve=()):
        nonlocal vmov
        if v in gpr_of:
            return gpr_of[v]
        free = [r for r in pool if r not in used]
        if not free:
            cands = [x for x in gpr_of if x not in reserve]
            if not cands:
                cands = list(gpr_of)
            victim = max(cands, key=lambda x: (
                next_use(x, i) - (SPILL_PENALTY if dirty[x] else 0),
                0 if dirty[x] else 1))
            reg = gpr_of.pop(victim)
            used.discard(reg)
            if dirty[victim]:
                out.append(f"vmov s{victim}, {reg}    @ spill x[{victim}]")
                vmov += 1
                dirty[victim] = False
            free = [reg]
        reg = free[0]
        used.add(reg)
        gpr_of[v] = reg
        out.append(f"vmov {reg}, s{v}    @ load x[{v}]")
        vmov += 1
        return reg

    for i, (dst, src) in enumerate(ops):
        rd = alloc(dst, i, reserve={dst, src})
        rs = alloc(src, i, reserve={dst, src})
        out.append(f"eor {rd}, {rd}, {rs}    @ x[{dst}] = x[{dst}] ^ x[{src}]")
        dirty[dst] = True

    for xv, reg in list(gpr_of.items()):
        if reg == 'r0':
            if dirty[xv]:
                out.append(f"vmov s{xv}, r0    @ spill before pop {{r0}}")
                vmov += 1
                dirty[xv] = False
            gpr_of.pop(xv)
            used.discard('r0')
    out.append("pop {r0}    @ restore output pointer")

    for offs, xv in stores:
        if xv in gpr_of:
            out.append(f"str  {gpr_of[xv]}, [r0, #{offs}]    @ y[{offs // 4}] = x[{xv}]")
        else:
            out.append(f"vstr.32 s{xv}, [r0, #{offs}]    @ y[{offs // 4}] = x[{xv}]")
    return out, vmov


SIM_VMOV_RS = re.compile(r'^vmov\s+(r\d+|r14|lr)\s*,\s*s(\d+)')
SIM_VMOV_SR = re.compile(r'^vmov\s+s(\d+)\s*,\s*(r\d+|r14|lr)')
SIM_EOR = re.compile(r'^eor\s+(r\d+|r14|lr)\s*,\s*(r\d+|r14|lr)\s*,\s*(r\d+|r14|lr)')
SIM_STR = re.compile(r'^(?:str|vstr\.32)\s+(\S+)\s*,\s*\[(r\d+|r14|lr)\s*,\s*#(\d+)\]')
SIM_POP_R0 = re.compile(r'^pop\s+\{r0\}')


def simulate_body(lines):
    S = {i: frozenset([i]) for i in range(32)}
    R = {}
    stores = {}
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith('@') or line.startswith('.'):
            continue
        line = re.sub(r'\s*@.*$', '', line)
        if not line:
            continue
        m = SIM_VMOV_RS.match(line)
        if m:
            R[m.group(1)] = S[int(m.group(2))]
            continue
        m = SIM_VMOV_SR.match(line)
        if m:
            S[int(m.group(1))] = R[m.group(2)]
            continue
        m = SIM_EOR.match(line)
        if m:
            rd, ra, rb = m.group(1), m.group(2), m.group(3)
            assert rd == ra, f"non-in-place eor: {line}"
            R[rd] = R[ra] ^ R[rb]
            continue
        m = SIM_STR.match(line)
        if m:
            src, base, offs = m.group(1), m.group(2), int(m.group(3))
            assert base == 'r0', f"unexpected base reg: {line}"
            stores[offs] = S[int(src[1:])] if src.startswith('s') else R[src]
            continue
        if SIM_POP_R0.match(line):
            R.pop('r0', None)
            continue
        raise AssertionError(f"unhandled instruction: {raw!r}")
    return stores


def find_prologue_end(lines):
    for i, line in enumerate(lines):
        if line.strip() == 'push {r0, r1}':
            return i + 1
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith('vldmia') and 'd0-d15' in s and i > 3:
            return i + 1
    return 0


def find_epilogue_start(lines):
    for i, line in enumerate(lines):
        if re.match(r'^pop\s+\{r1\}\s*$', line.strip()):
            return i
    for i, line in enumerate(lines):
        if 'vpop' in line.lower() and 'd8-d15' in line.lower():
            return i
    return len(lines)
