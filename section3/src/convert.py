#!/usr/bin/env python3
"""Convert XOR-sequence text files (matrix + reduced permutation + in-place XOR
list) into an orders JSON and an annotated baseline .s. Each conversion is
checked against the file's Original Matrix."""
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lib_core
import lib_regen
from lib_core import emit_count
from lib_regen import emit, simulate_body

BIT_REV = False

SCAFFOLD_HEAD = """\
.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.global gft_mul_v{n}
.type gft_mul_v{n}, %function
.align 2
gft_mul_v{n}:
stmdb   sp!, {{r4-r11, lr}}
sub     sp, sp, #4
vpush   {{d8-d15}}
vldmia  r1, {{d0-d15}}
push {{r0, r1}}"""

SCAFFOLD_TAIL = """\
pop     {{r1}}
vpop    {{ d8-d15 }}
add     sp, sp, #4
ldmia   sp!, {{r4-r11, pc}}
.size   gft_mul_v{n}, .-gft_mul_v{n}"""


def parse_file(path):
    lines = [l.rstrip() for l in path.read_text(encoding="utf-8").splitlines()]

    def read_matrix(label):
        idx = next(i for i, l in enumerate(lines) if l.startswith(label))
        rows, i = [], idx + 1
        while len(rows) < 32:
            l = lines[i].strip()
            if re.fullmatch(r"[01]{32}", l):
                rows.append(l)
            i += 1
        return rows

    orig = read_matrix("Original Matrix")
    ops, yann = [], {}
    for l in lines:
        m = re.fullmatch(
            r"x\[(\d+)\]\s*=\s*x\[(\d+)\]\s*\^\s*x\[(\d+)\]\s*;?"
            r"(?:\s+y\[(\d+)\]\s*;?)?\s*", l.strip())
        if m:
            d, d2, s = int(m.group(1)), int(m.group(2)), int(m.group(3))
            assert d == d2, f"non-in-place line: {l}"
            ops.append((d, s))
            if m.group(4) is not None:
                yann[int(m.group(4))] = d
    return orig, ops, yann


def row_set(rstr):
    return frozenset(pos if not BIT_REV else 31 - pos
                     for pos, ch in enumerate(rstr) if ch == '1')


def matrix_of(ops, n=32):
    m = [frozenset([i]) for i in range(n)]
    for d, s in ops:
        m[d] = m[d] ^ m[s]
    return m


def convert(path, num):
    orig, ops, yann = parse_file(path)
    assert len(yann) == 32, f"{path.name}: incomplete y[] annotations"
    assert max(max(d, s) for d, s in ops) <= 31, f"{path.name}: index > 31"

    want = [row_set(orig[k]) for k in range(32)]
    m = matrix_of(ops)
    for k in range(32):
        assert m[yann[k]] == want[k], f"{path.name}: y[{k}] mismatch"

    stores = [(4 * k, yann[k]) for k in range(32)]
    body, ev = emit(ops, stores)
    assert ev == emit_count(ops, stores), f"{path.name}: emit != emit_count"
    assert simulate_body(body) == {4 * k: want[k] for k in range(32)}, f"{path.name}: verify FAIL"

    name = re.sub(r"^vi_mul_\d+_|\.txt$", "", path.name)
    entry = {"name": name, "eor": len(ops), "vmov": ev, "total": len(ops) + ev,
             "ops": [list(o) for o in ops],
             "stores": [list(s) for s in stores], "base": "r0"}
    fn_text = (SCAFFOLD_HEAD.format(n=num) + "\n" + "\n".join(body) + "\n" +
               SCAFFOLD_TAIL.format(n=num) + "\n")
    return name, entry, fn_text


def assign_nums(files):
    # s_inv_N -> vN; any other sequence -> next free number from 11
    nums, extra = {}, 11
    for p in sorted(files, key=lambda p: p.name):
        n = re.sub(r"^vi_mul_\d+_|\.txt$", "", p.name)
        mm = re.match(r"s_inv_(\d+)", n)
        if mm:
            nums[p] = int(mm.group(1))
        else:
            nums[p] = extra
            extra += 1
    return nums


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sequences_dir")
    ap.add_argument("out_json")
    ap.add_argument("out_baseline")
    ap.add_argument("--penalty", type=int, default=11)
    args = ap.parse_args()
    lib_core.SPILL_PENALTY = args.penalty
    lib_regen.SPILL_PENALTY = args.penalty

    files = list(Path(args.sequences_dir).glob("*.txt"))
    nums = assign_nums(files)
    out, fns = {}, []
    print(f"{'vN':<5}{'name':<14}{'eor':>6}{'vmov':>6}{'total':>7}")
    print("-" * 42)
    for path in sorted(files, key=lambda p: nums[p]):
        num = nums[path]
        name, entry, fn_text = convert(path, num)
        out[f"v{num}"] = entry
        fns.append(fn_text)
        print(f"v{num:<4}{name:<14}{entry['eor']:>6}{entry['vmov']:>6}{entry['total']:>7}")
    Path(args.out_json).write_text(json.dumps(out), encoding="utf-8")
    Path(args.out_baseline).write_text("\n".join(fns), encoding="utf-8")
    print(f"json     -> {args.out_json}")
    print(f"baseline -> {args.out_baseline}")


if __name__ == "__main__":
    main()
