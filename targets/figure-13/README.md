# Figure 13 — Effectiveness of TGS with P_norm = 0.99 and f_i = 2

> Fig. 13: Effectiveness of TGS with P_norm = 0.99 and f_i = 2. The first two
> subfigures show the probability of T_robust ≥ 30 days, and the last two the
> normalized mean time of robustness (T_robust) within 30 days.
> Without TGS: **0.486** (probability), **0.505** (normalized mean).

[Figure 12](../figure-12/README.md) with the per-region fault bound raised from
one to two. Each region now has 5 nodes and runs 3 replicas per task, so 9
message pairs per round instead of 4.

## Generate it

```bash
./run.sh figure-13
```

Runs the two Monte-Carlo sweeps at the paper's parameters — 10,000 runs per (α, β) over a 30-day horizon — into
`results/tgs/`, and plots them into `outputs/figure-13.pdf` / `.png`.
**Budget about 4.5 hours on 16 cores.**

Afterwards, `./compare.py tgs` reports how far the result is from the authors'
recorded runs.

Roughly twice the cost of Figure 12: 9 message pairs per round against 4.

## Parameters

| parameter | value |
|---|---|
| P_norm | 0.99 |
| f_i | **2** → 5 nodes per region, 3 replicas per task |
| horizon | 86400 × 30 rounds |
| α grid | 0.001, 0.01, 0.1, 0.5, 1.0 (adaptive) · plus 0.2 (aggressive) |
| β grid | 5, 10, 20, 30, 50 (adaptive) · 1, 2, 5, 10, 20, 30, 50 (aggressive) |

Note the grids differ from Figures 6 and 12 — β = 3 is dropped and β = 30
added. With f_i = 2 the threshold (f_i+1)·s_pen ≥ 1 sits at β ≤ 3, so β = 3 is
exactly the degenerate point where the adaptive adversary stops being adaptive.

## Reading the result

More faults per region makes the system *more* robust here, not less, and both
with and without TGS (the baseline rises from 0.108 to 0.486). The reason is
that f_i+1 replicas per task means more independent chances that at least one
message per task is delivered on time in a given round, and the run only ends
when *no* pair delivers. TGS then adds its usual margin on top: at
(α, β) = (0.01, 5) the pair survives a month with probability 0.998 against
both adversaries.

The α = 1.0 row is worth comparing against Figure 12: the false-positive
collapse that dominated at f_i = 1 is much milder at f_i = 2, though the flag
counters show it is still happening (tens of thousands of flags per run against
~3 in the well-tuned settings). The extra replica absorbs the churn.
