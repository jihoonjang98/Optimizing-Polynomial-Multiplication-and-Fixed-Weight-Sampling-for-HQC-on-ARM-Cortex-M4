#!/usr/bin/env python3
"""Merge orders JSONs into a target, keeping the per-function minimum eor+vmov
(verified against the baseline store map). The target is updated in place."""
import argparse
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lib_core
import lib_regen
from lib_core import emit_count, iter_functions, parse_function
from lib_regen import emit, find_epilogue_start, find_prologue_end, simulate_body


def score(entry):
    ops = [tuple(o) for o in entry['ops']]
    stores = [tuple(s) for s in entry['stores']]
    body, ev = emit(ops, stores)
    assert ev == emit_count(ops, stores)
    return ops, stores, ev, len(ops) + ev, simulate_body(body)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('baseline')
    ap.add_argument('target')
    ap.add_argument('cands', nargs='+')
    ap.add_argument('--penalty', type=int, default=None)
    args = ap.parse_args()

    if args.penalty is not None:
        lib_core.SPILL_PENALTY = args.penalty
        lib_regen.SPILL_PENALTY = args.penalty

    bmap = {}
    for fn, body in iter_functions(Path(args.baseline).read_text(
            encoding='utf-8', errors='replace')):
        ops, st = parse_function(body)
        if len(ops) < 10 or len(st) != 32:
            continue
        fl = body.splitlines()
        bmap[f'v{fn}'] = simulate_body(fl[find_prologue_end(fl):find_epilogue_start(fl)])

    tgt_path = Path(args.target)
    target = json.loads(tgt_path.read_text(encoding='utf-8')) if tgt_path.exists() else {}
    cands = [json.loads(Path(c).read_text(encoding='utf-8')) for c in args.cands]

    print(f"penalty={lib_core.SPILL_PENALTY}, target={tgt_path.name}")
    print(f"{'fn':<6}{'cur':>7}{'new':>7}{'d':>6}  src")
    print("-" * 40)

    final = dict(target)
    improved = []
    scur = snew = 0
    keys = sorted({k for k in bmap} & (set(target) | {k for c in cands for k in c}),
                  key=lambda k: int(k[1:]))
    for k in keys:
        cur_map = bmap[k]
        best_tot, best, best_src = float('inf'), None, None
        pool = ([('target', target[k])] if k in target else []) + \
               [(Path(args.cands[i]).name, c[k]) for i, c in enumerate(cands) if k in c]
        for src, entry in pool:
            ops, stores, ev, tot, m = score(entry)
            if m != cur_map or len(m) != 32:
                continue
            if tot < best_tot:
                best_tot, best, best_src = tot, (ops, stores, ev), src
        if best is None:
            continue
        ops, stores, ev = best
        cur_tot = score(target[k])[3] if k in target else None
        final[k] = {'eor': len(ops), 'vmov': ev, 'total': len(ops) + ev,
                    'ops': [list(o) for o in ops], 'stores': [list(s) for s in stores]}
        if cur_tot is not None:
            scur += cur_tot
            snew += best_tot
            if best_tot < cur_tot and best_src != 'target':
                improved.append(k)
            print(f"{k:<6}{cur_tot:>7}{best_tot:>7}{best_tot - cur_tot:>+6}  "
                  f"{best_src if best_src != 'target' else '(keep)'}")
        else:
            print(f"{k:<6}{'-':>7}{best_tot:>7}{'':>6}  {best_src} (new)")

    if tgt_path.exists():
        shutil.copy(tgt_path, tgt_path.with_suffix('.json.bak'))
    tgt_path.write_text(json.dumps(final), encoding='utf-8')
    print("-" * 40)
    print(f"{'SUM':<6}{scur:>7}{snew:>7}{snew - scur:>+6}")
    print(f"improved {len(improved)}: {improved}")
    print(f"written -> {tgt_path}")


if __name__ == '__main__':
    main()
