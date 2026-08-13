# Figure 7 — Normalized mean T_robust with TGS

> Fig. 7: Normalized mean time T_robust with TGS. Normalized mean time without TGS is **0.268**.

Same experiment as [Figure 6](../figure-06/README.md) — same two sweeps, same
raw data — but plotting the mean time spent in normal operation as a fraction
of the 30-day horizon instead of the fraction of runs that survived it.

## Generate it

```bash
./run.sh figure-07
```

The two targets share a dataset, so if you have already run `figure-06` this
reuses the cached `results/tgs/p999-f1-*.csv` and only replots — seconds.
Otherwise it generates them, ~3.5 hours. `--force` recomputes.

## Why both metrics

`survive_rate` (Fig. 6) is binary per run — did the pair reach 30 days at all.
`avg_survive_time` (Fig. 7) is graded, so it separates configurations that all
fail from those that fail late. The gap between the two is where the parameter
choice buys time rather than immunity: at α = 1.0, β = 3 only 93.8% of runs
survive, but the mean lifetime is still 96.8% of the horizon.

Simulation parameters, adversary models and the "without TGS" baseline are
documented in [figure-06/README.md](../figure-06/README.md).
