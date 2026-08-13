# Figure 6 — Pr(T_robust ≥ 30 days) with TGS

> Fig. 6: Probability of T_robust ≥ 30 days with TGS (f_i = 1). Probability without TGS is **0.143**.

Two heat maps over the TGS parameters (α, β), one per adversary, showing the
fraction of 30-day simulations in which the region pair never had to leave
normal operation.

## Generate it

```bash
./run.sh figure-06
```

Runs the two Monte-Carlo sweeps at the paper's parameters — 10,000 runs per (α, β) over a 30-day horizon —
into `results/tgs/`, and plots them into `outputs/figure-06.pdf` and `.png`.

**Budget about 2.3 hours on 16 cores.** The sweeps are cached, so
[figure-07](../figure-07/README.md), [figure-08](../figure-08/README.md) and
[table-03](../table-03/README.md) — which read the same data — cost nothing
extra afterwards. `--force` recomputes.

Afterwards, `./compare.py tgs` reports how far the result is from the authors'
recorded runs.

## What is simulated

`src/tgs/tgs_sim.py`, one run per grid point:

| parameter | value |
|---|---|
| P_norm | 0.999 |
| f_i (both regions) | 1 → 3 nodes per region, 2 replicas per task |
| horizon | 86400 × 30 rounds (30 days at one heartbeat per second) |
| runs per (α, β) | 10,000 |
| α grid | 0.001, 0.01, 0.1, 0.2, 0.5, 1.0 |
| β grid | 3, 5, 10, 20, 50 (adaptive) · 1, 2, 3, 5, 10, 20, 50 (aggressive) |

Each round, every one of the (f_i+1)² primary pairs exchanges a message.
A message is suspicious if the network misbehaves (probability 1 − P_norm) or
if a Byzantine endpoint attacks; suspicious costs both endpoints s_pen = 1/β,
normal earns them s_awd, capped at 1. A node whose score hits 0 is flagged and
replaced. The run ends the first round in which *no* pair delivered — that is
the safe-mode transition.

The two adversaries differ only in when a Byzantine node attacks:

- **aggressive** — on every message it is assigned.
- **adaptive** — only while its score is still above 2·s_pen, so it stops
  before being flagged. When (f_i+1)·s_pen ≥ 1 a single round drains a score
  anyway, so backing off gains nothing and it behaves aggressively.

## The "without TGS" number

The 0.143 in the caption is the same simulation with flagging and reassignment
disabled. With TGS off, the aggressive adversary's decisions no longer depend
on scores, so the value is independent of (α, β) and one grid point suffices.
It is computed into `results/tgs/no-tgs-p999-f1.csv` alongside the sweeps.

## Reading the result

Robustness is non-monotonic in both parameters. Overly strict settings (high α,
small β) flag nodes on ordinary network jitter, and each false positive
reassigns the task — which is itself a window of exposure. Overly loose
settings never flag the real attacker. The paper's operating point is
(α, β) = (0.1, 5).

## Origin of this code

`micros/reputation/adaptive-attack.py` and `aggressive-attack.py`, whose
`main()` hard-coded P_norm, f, n, the grids and the `use_flag` switch; the two
files also differed by a hand-edited `if 1:` in the attack decision. Both are
now one parameterised simulator, with the per-figure settings recorded in
`common/tgs_target.py`. Untouched originals are under `src/original/`.
