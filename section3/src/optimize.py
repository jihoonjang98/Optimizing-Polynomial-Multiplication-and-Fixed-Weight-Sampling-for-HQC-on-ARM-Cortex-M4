#!/usr/bin/env python3
"""VMOV optimization: one simulated-annealing run per seed (parallel), keep best."""
import argparse
import json
import os
import random
import sys
import time
from multiprocessing import Pool
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lib_core
from lib_core import (anneal, emit_count, greedy_start, iter_functions,
                      parse_function, random_start)


def save(path, data):
    tmp = path.with_suffix('.json.tmp')
    tmp.write_text(json.dumps(data), encoding='utf-8')
    tmp.replace(path)


def _run_seed(task):
    start, stores, iters, seed = task
    o, v = anneal(start, stores, iters=iters, seed=seed)
    return v, o


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('baseline')
    ap.add_argument('out')
    ap.add_argument('--iters', type=int, default=1280000)
    ap.add_argument('--seed', default=None)
    ap.add_argument('--fns', default='all')
    ap.add_argument('--start', choices=['baseline', 'random', 'greedy'], default='greedy')
    ap.add_argument('--penalty', type=int, default=None)
    args = ap.parse_args()

    if args.penalty is not None:
        os.environ['SPILL_PENALTY'] = str(args.penalty)   # inherited by spawn workers
        lib_core.SPILL_PENALTY = args.penalty

    seeds = [None] if args.seed is None else [int(x) for x in args.seed.split(',')]
    text = Path(args.baseline).read_text(encoding='utf-8', errors='replace')
    sel = None if args.fns == 'all' else {int(x) for x in args.fns.split(',')}
    out_path = Path(args.out)
    out = json.loads(out_path.read_text(encoding='utf-8')) if out_path.exists() else {}

    seedstr = 'none' if args.seed is None else str(seeds)
    print(f"iters={args.iters}, seeds={seedstr} ({len(seeds)} jobs), "
          f"start={args.start}, penalty={lib_core.SPILL_PENALTY}")
    print(f"{'fn':<6}{'eor':>6}{'vmov0':>7}{'vmov1':>7}{'d':>5}  per-seed  (time)")
    print("-" * 58)

    with Pool(processes=len(seeds)) as pool:
        for fn, body in iter_functions(text):
            if sel is not None and fn not in sel:
                continue
            ops, stores = parse_function(body)
            if len(ops) < 10 or len(stores) != 32:
                continue
            vmov0 = emit_count(ops, stores)
            t0 = time.time()

            if args.start == 'greedy':
                g = greedy_start(ops)
                starts = [g] * len(seeds)
            elif args.start == 'random':
                starts = [random_start(ops, random.Random(sd)) for sd in seeds]
            else:
                starts = [list(ops)] * len(seeds)

            tasks = [(starts[k], stores, args.iters, seeds[k]) for k in range(len(seeds))]
            results = pool.map(_run_seed, tasks)
            per_seed = [v for v, _ in results]
            best_v, best_o = min(results, key=lambda r: r[0])

            key = f"v{fn}"
            prev = out.get(key)
            if prev is None or best_v < prev.get('vmov', float('inf')):
                out[key] = {'eor': len(ops), 'vmov': best_v,
                            'total': len(ops) + best_v, 'ops': best_o, 'stores': stores}
            save(out_path, out)
            print(f"{key:<6}{len(ops):>6}{vmov0:>7}{best_v:>7}"
                  f"{best_v - vmov0:>+5}  {per_seed}  ({time.time() - t0:.0f}s)", flush=True)

    tot = sum(e['total'] for e in out.values())
    print("-" * 58)
    print(f"file total: {tot}")
    print(f"saved -> {out_path}")


if __name__ == '__main__':
    main()
