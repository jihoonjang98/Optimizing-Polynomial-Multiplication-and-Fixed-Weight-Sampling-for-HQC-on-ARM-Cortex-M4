# XOR-Reordering VMOV Optimization

Reduces VMOV (VFP spill/reload) instructions in Cortex-M4 GF-multiplication
assembly by reordering commuting XOR operations under a register-allocation cost
model. Python 3.8+.

## Optimize

```
python ./src/optimize.py ./data/hqc-1/baseline.s out.json --penalty 11 --seed 11,22,33,44,55,67,78,89,90 --iters 8000000
```

`--seed` runs one annealing job per seed in parallel and keeps the best.
`--iters` sets the annealing iterations per seed (default 1280000).
Use `data/hqc-3/baseline.s` or `data/hqc-5/jang/baseline.s` for the other levels.

## Regenerate assembly from the result

```
python src/regen.py data/hqc-5/new/baseline.s out.json out.s --penalty 11
```
