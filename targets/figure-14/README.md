# Figure 14 — Effectiveness of TGS under network DoS

> Fig. 14: Effectiveness of TGS under network DoS (f_i = 1). The first two
> subfigures show the probability of T_robust ≥ 30 days, and the last two the
> normalized mean time of robustness (T_robust) within 30 days.
> Without TGS: **0.108** (probability), **0.112** (normalized mean).

What happens when the probabilistic bounded-jitter assumption is violated at
run time. TGS is configured with P_norm = 0.999 — the value measured on the
FABRIC testbed — but a DoS attack on the inter-region network degrades actual
delivery to P_norm = 0.99, a ten-fold increase in jitter. The reward/penalty
ratio is now mis-calibrated for the traffic the nodes actually see.

This is the distinguishing detail of this figure: **s_awd and s_pen are derived
from 0.999 while the events are drawn at 0.99.** In `src/tgs/tgs_sim.py` that
is `--p-norm 0.999 --p-actual 0.99`.

## Generate it

```bash
./run.sh figure-14
```

Runs the two Monte-Carlo sweeps at the paper's parameters — 10,000 runs per (α, β) over a 30-day horizon — into
`results/tgs/`, and plots them into `outputs/figure-14.pdf` / `.png`.
**Budget about 0.8 hours on 16 cores.**

Afterwards, `./compare.py tgs` reports how far the result is from the authors'
recorded runs.

## Parameters

| parameter | value |
|---|---|
| P_norm configured | 0.999 |
| P_norm delivered | **0.99** |
| f_i | 1 → 3 nodes per region, 2 replicas per task |
| horizon | 86400 × 30 rounds |
| α grid | 0.001, 0.01, 0.1, 0.5, 1.0 (adaptive) · plus 0.2 (aggressive) |
| β grid | 3, 5, 10, 20, 50 (adaptive) · 1, 2, 3, 5, 10, 20, 50 (aggressive) |

## Reading the result

TGS remains effective, but only in the low-α corner. With s_awd sized for a
0.999 network, a node's expected score drift is
`p_actual · s_awd − (1 − p_actual) · s_pen`; substituting the mis-calibrated
s_awd, the drift turns negative around α ≈ 0.1, β = 3 — and the measured
survival there is 0.0001. Below that, at α = 0.01, β = 5, the pair still
survives a month with probability 0.966.

Two consequences the paper draws from this:

- Operators should configure conservatively (small α) when the network's
  P_norm may degrade, since the cost of being too lenient is graceful while the
  cost of being too strict under DoS is a cliff.
- Only *operational robustness* degrades. HEIMDALLR's safety properties —
  consensus on latency, consensus on correctness, and bounded-time recovery
  (Theorems 2, 4, 5) — do not depend on the accuracy of the latency estimate,
  so they are unaffected.

The baseline (0.108 / 0.112) is the no-TGS run at the delivered P_norm = 0.99,
which is why it equals Figure 12's rather than Figure 6's.
