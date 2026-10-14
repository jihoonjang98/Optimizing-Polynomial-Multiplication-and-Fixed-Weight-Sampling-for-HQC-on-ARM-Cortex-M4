#!/usr/bin/env python3
"""Count EOR/VMOV per function in asm files."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from lib_core import iter_functions


def count(path):
    res = {}
    for fn, body in iter_functions(path.read_text(encoding='utf-8', errors='replace')):
        eor = len(re.findall(r'^\s*eor\b', body, re.M))
        vmov = len(re.findall(r'^\s*vmov(?:\.\w+)?\b', body, re.M))
        res[fn] = (eor, vmov)
    return res


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: count.py <file.s> [...]")
    for arg in sys.argv[1:]:
        path = Path(arg)
        res = count(path)
        print(f"== {path.name} ==")
        print(f"{'fn':<6}{'EOR':>6}{'VMOV':>7}{'total':>7}")
        te = tv = 0
        for fn in sorted(res):
            e, v = res[fn]
            te += e
            tv += v
            print(f"v{fn:<5}{e:>6}{v:>7}{e + v:>7}")
        print(f"{'SUM':<6}{te:>6}{tv:>7}{te + tv:>7}\n")


if __name__ == '__main__':
    main()
