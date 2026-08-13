# Figure 8 — Pr(T_robust ≥ 30 days) vs number of region pairs

> Fig. 8: Pr(T_robust ≥ 30 days) with varying number of regions.

Scalability: a deployment stays in normal operation only while *every* region
pair does. Region pairs fail independently, so the system-wide probability
over N pairs is the single-pair probability raised to N.

## Generate it

```bash
./run.sh figure-08
```

**The figure itself is a calculation, not a simulation** — `p^n`, exactly as in
the paper. Its *inputs* are real, though: the Figure 6 sweeps and the Table 3
PISTIS runs, which this target generates like any other (or reuses from cache).
So it costs whatever those cost — seconds if you have already run
[figure-06](../figure-06/README.md) and [table-03](../table-03/README.md),
several hours if not.

> **This figure is unusually sensitive to its inputs' precision.** Raising a
> probability to the 200th power multiplies its relative uncertainty by roughly
> the same factor:
>
> | runs/point | single-pair Pr | σ(Pr) | Pr at 200 pairs | relative uncertainty |
> |---|---|---|---|---|
> | 500 | 0.9960 | 0.0028 | 0.449 | 57 % |
> | 10,000 (the paper's, and what this runs) | 0.9994 | 0.0002 | 0.887 | 5 % |
>
> This is why the sample count is not adjustable: at anything less than the
> paper's, the HEIMDALLR curve lands near 0.45 instead of 0.887 — not a bug,
> just an estimate too coarse to exponentiate. `generate.py` prints the
> propagated uncertainty each run.

## The sweep starts at 5 pairs, not 1

`NUM_REGION_PAIRS = [5, 10, 25, 50, 100, 150, 200]`, matching the original
script. This matters more than it looks: at n = 1 each series is just its raw
single-pair probability, so PISTIS at T = 12d starts at **0.534** and the curve
descends from halfway up the plot. The published figure has that series
entering at **0.534⁵ = 0.043**, hugging the axis from the start — a visibly
different figure. Reading the T = 14d series off the paper confirms the sweep:
0.895 / 0.573 / 0.329 / 0.108 at n = 5 / 25 / 50 / 100, which is 0.978ⁿ.

## The single-pair probabilities

They are exactly the ones in [Table 3](../table-03/README.md):

| series | single-pair Pr | Pr at 200 pairs |
|---|---|---|
| Heimdallr (T = d), (α, β) = (0.1, 5) | 0.9994 | **0.887** |
| PISTIS (T = 16d) | 0.9992 | 0.852 |
| PISTIS (T = 14d) | 0.9780 | 0.012 |
| PISTIS (T = 12d) | 0.5340 | ~0 |

The paper quotes 0.886 for HEIMDALLR at 200 pairs. The point is not the
absolute value but the exponent: a per-pair probability that looks adequate at
one pair (0.978, PISTIS at 14d) collapses at deployment scale, and HEIMDALLR
holds its margin with a worst-case recovery time of just over two heartbeat
periods rather than PISTIS's 3T = 48 d.

## Implementation note

This figure is the one target with no simulation of its own — it is an
extrapolation from Table 3. The original code
(`micros/reputation/Untitled-1.ipynb`, cell 9) hard-coded the four base
probabilities as literals; here they are read from whichever dataset the mode
selects, so the curve moves if the underlying runs do.
