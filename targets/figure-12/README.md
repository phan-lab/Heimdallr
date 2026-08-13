# Figure 12 — Effectiveness of TGS with P_norm = 0.99 and f_i = 1

> Fig. 12: Effectiveness of TGS with P_norm = 0.99 and f_i = 1. Subfigures (a)–(b)
> show the probability of T_robust ≥ 30 days, and (c)–(d) the normalized mean of
> T_robust over a 30-day simulation horizon.
> Without TGS: **0.108** (probability), **0.112** (normalized mean).

The [Figure 6/7](../figure-06/README.md) experiment on a ten-times less
reliable network: P_norm drops from 0.999 to 0.99, so ordinary jitter alone
produces ten times more suspicious messages and it is correspondingly harder to
separate an attacker from the network.

Four panels: probability and normalized mean time, each under both adversaries.

## Generate it

```bash
./run.sh figure-12
```

Runs the two Monte-Carlo sweeps at the paper's parameters — 10,000 runs per (α, β) over a 30-day horizon — into
`results/tgs/`, and plots them into `outputs/figure-12.pdf` / `.png`.
**Budget about 1.6 hours on 16 cores.**

Afterwards, `./compare.py tgs` reports how far the result is from the authors'
recorded runs.

## Parameters

Identical to Figure 6 except for P_norm:

| parameter | value |
|---|---|
| P_norm | **0.99** |
| f_i | 1 → 3 nodes per region, 2 replicas per task |
| horizon | 86400 × 30 rounds |
| α grid | 0.001, 0.01, 0.1, 0.2, 0.5, 1.0 |
| β grid | 3, 5, 10, 20, 50 (adaptive) · 1, 2, 3, 5, 10, 20, 50 (aggressive) |

## Reading the result

TGS still works, but the usable region of the parameter space narrows sharply.
At (α, β) = (0.1, 5) the pair stays in normal operation for a month with
probability ≥ 0.956 and normalized robustness ≥ 0.970 under both adversaries —
against 0.108 with TGS switched off.

The failure modes at the edges are visible and are opposite in kind:

- **α = 1.0** (heavy penalty per suspicious message) — false positives on
  ordinary jitter. Every flagged correct node forces a task reassignment, and
  those reassignments are themselves exposure. The whole top row degrades.
- **β = 1** under the aggressive adversary — a single penalty drains a full
  score, so a node is flagged on its first suspicious message and the system
  thrashes. Note this column is absent from the adaptive grids: with
  (f_i+1)·s_pen ≥ 1 an adaptive attacker cannot dodge flagging anyway, so it
  degenerates to the aggressive case.
- **α = 0.001, small β** — rewards accumulate so slowly relative to penalties
  that even correct nodes drift down.

The "without TGS" baseline (0.108 / 0.112) is the same simulation with flagging
and reassignment disabled; see [figure-06](../figure-06/README.md#the-without-tgs-number).
The recorded run is in `data/reference/tgs/no-tgs-p99-f1.txt`, which also shows
directly that the value does not depend on (α, β).
