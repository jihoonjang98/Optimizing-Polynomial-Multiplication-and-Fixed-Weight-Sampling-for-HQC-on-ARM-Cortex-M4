# Optimizing Polynomial Multiplication and Fixed-Weight Sampling for HQC on ARM Cortex-M4

Source code accompanying the paper of the same title, IACR Transactions on
Cryptographic Hardware and Embedded Systems (TCHES), Volume 2027, Issue 1.

This repository contains (i) the optimized HQC implementations for the ARM
Cortex-M4 evaluated in the paper and (ii) the tooling that produces the
VMOV-minimized butterfly assembly of Section 3.

## Contents

```
HQC_m4/
  hqc-1/  hqc-3/  hqc-5/                       Optimized HQC, no caching
  cache_hqc-1/  cache_hqc-3/  cache_hqc-5/     Same, with public-key caching (Section 5)
vmov_optimizer/
  src/                                         XOR-reordering VMOV optimizer (Section 3)
  data/                                        Baseline and optimized XOR sequences
  Case_AESmixcolumns.pdf                       Supplementary note: the AES MixColumns case study
```

`hqc-1`, `hqc-3`, and `hqc-5` correspond to HQC at NIST security levels 1, 3,
and 5. Each `HQC_m4/<name>/m4f/` directory is a self-contained pqm4
`crypto_kem` implementation (the `m4f` variant). The implementations follow
HQC specification release v5.0.0 (2025-08-22).

## Building, testing, and benchmarking

The implementations plug into the pqm4 benchmarking framework
(https://github.com/mupq/pqm4, commit a24bb4b).

1. Copy `HQC_m4/<name>` into the pqm4 checkout so the sources live at
   `crypto_kem/<name>/m4f/`.
2. Check the known-answer tests:
   `python3 testvectors.py -p nucleo-l4r5zi <name> --uart /dev/ttyACM0`
   `python3 test.py -p nucleo-l4r5zi <name> --uart /dev/ttyACM0`
3. Benchmark:
   `python3 benchmarks.py -p nucleo-l4r5zi <name> --uart /dev/ttyACM0`

Reported figures were obtained with:

- Board: STM32 NUCLEO-L4R5ZI (Cortex-M4, 2 MB Flash, 640 KB SRAM)
- Compiler: arm-none-eabi-gcc 10.3.1, `-O3`
- Clock: 20 MHz (zero wait states)

Cycle counts come from the pqm4 `speed` benchmark and peak stack from the
`stack` benchmark. Flash and RAM are read with `arm-none-eabi-size` on
`crypto_kem_<name>_m4f_speed.elf`, where `text + data` is the Flash size and
`data + bss` is the RAM size.

## Mapping to the paper

- `HQC_m4/hqc-{1,3,5}` produce the "Ours" figures of the polynomial
  multiplication, speed, component, and memory tables.
- `HQC_m4/cache_hqc-{1,3,5}` produce the "Caching" column of the caching table
  and the "Ours (caching)" rows of the memory table.
- `vmov_optimizer/src` implements the dirty-aware register allocation and
  XOR-operation reordering of Section 3. `vmov_optimizer/data/<level>/baseline.s` are
  the input XOR sequences and `reorders.s` the optimized assembly whose
  per-function EOR and VMOV counts appear in Table 5. See `vmov_optimizer/README.md`.
- `vmov_optimizer/Case_AESmixcolumns.pdf` is a short supplementary note that applies the
  same optimization to the 92-XOR sequence of AES MixColumns, the example
  mentioned in the conclusion of the paper, with the full instruction listings.

## License and third-party code

See `LICENSE`. The FAFFT, radix-16, and RS/RM decoder code in `HQC_m4` builds on
the public-domain implementations of Chen, Chou, and Krausz and of Chen et al.
(TCHES 2026), and on the HQC reference implementation; the remaining parts of
`HQC_m4` and all of `vmov_optimizer` are by the authors of the paper.
