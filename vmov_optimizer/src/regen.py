#!/usr/bin/env python3
"""Regenerate optimized asm from an orders JSON, with symbolic verification."""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lib_core
import lib_regen
from lib_core import FN_PAT, parse_function
from lib_regen import emit, find_epilogue_start, find_prologue_end, simulate_body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('baseline')
    ap.add_argument('orders')
    ap.add_argument('out')
    ap.add_argument('--penalty', type=int, default=None)
    args = ap.parse_args()

    if args.penalty is not None:
        lib_regen.SPILL_PENALTY = args.penalty
        lib_core.SPILL_PENALTY = args.penalty

    src, orders_path, dst = Path(args.baseline), Path(args.orders), Path(args.out)
    orders = json.loads(orders_path.read_text(encoding='utf-8'))
    text = src.read_text(encoding='utf-8', errors='replace')
    print(f"penalty = {lib_regen.SPILL_PENALTY}")

    parts = []
    prev_end = 0
    tot_eor = tot_vmov = 0
    print(f"{'function':<14}{'eor':>6}{'vmov':>7}{'total':>7}{'verified':>10}")
    print("-" * 46)
    while True:
        m = FN_PAT.search(text, prev_end)
        if not m:
            parts.append(text[prev_end:])
            break
        parts.append(text[prev_end:m.start()])
        fn_body = m.group(1)
        fn_num = int(m.group(2))
        fn_lines = fn_body.splitlines()
        key = f"v{fn_num}"

        base_ops, base_stores = parse_function(fn_body)
        if key not in orders or len(base_ops) < 10 or len(base_stores) != 32:
            parts.append(fn_body)
            prev_end = m.end()
            continue

        entry = orders[key]
        ops = [tuple(o) for o in entry['ops']]
        stores = [tuple(s) for s in entry['stores']]
        assert [o for o, _ in stores] == [o for o, _ in base_stores]

        prologue_end = find_prologue_end(fn_lines)
        epilogue_start = find_epilogue_start(fn_lines)
        new_body, vmov = emit(ops, stores)

        ref = simulate_body(fn_lines[prologue_end:epilogue_start])
        new = simulate_body(new_body)
        if not (ref == new and len(ref) == 32):
            raise AssertionError(f"{key}: verification FAILED")

        new_fn = ('\n'.join(fn_lines[:prologue_end]) + '\n' +
                  '\n'.join(new_body) + '\n' +
                  '\n'.join(fn_lines[epilogue_start:]))
        if not new_fn.endswith('\n'):
            new_fn += '\n'
        parts.append(new_fn)
        tot_eor += len(ops)
        tot_vmov += vmov
        print(f"{'gft_mul_v' + str(fn_num):<14}{len(ops):>6}{vmov:>7}"
              f"{len(ops) + vmov:>7}{'OK':>10}")
        prev_end = m.end()

    dst.write_text(''.join(parts), encoding='utf-8')
    print("-" * 46)
    print(f"{'TOTAL':<14}{tot_eor:>6}{tot_vmov:>7}{tot_eor + tot_vmov:>7}")
    print(f"output: {dst}")


if __name__ == '__main__':
    main()
