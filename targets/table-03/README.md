# Table 3 — HEIMDALLR vs PISTIS operational robustness

> TABLE III: HEIMDALLR vs PISTIS operational robustness.

| System | HEIMDALLR (T = d) | PISTIS (T = 8d) | (T = 12d) | (T = 14d) | (T = 16d) |
|---|---|---|---|---|---|
| Pr(T_robust ≥ 30 days) | 0.999 | 0.000 | 0.534 | 0.978 | 0.999 |
| Normalized mean T_robust | 0.999 | 0.003 | 0.742 | 0.988 | 0.999 |

Both systems face the same question — how long does a region pair stay in
normal operation before something forces a safe-mode transition — but they get
there differently, so the table combines two separate simulators.

## Generate it

```bash
./run.sh table-03
```

Writes `outputs/table-03.md`. **Budget 5–6 hours on 16 cores** — see below.

## Where each number comes from

**HEIMDALLR** is the TGS Monte-Carlo of [Figure 6](../figure-06/README.md) read
at the paper's operating point (α, β) = (0.1, 5), P_norm = 0.999, f_i = 1. The
table reports the worst case over the two adversaries; the generated file also
prints them separately (adaptive 0.9994, aggressive 0.9996).

**PISTIS** is a separate round simulator, `src/pistis-sim/`, built and run by
this target. Seven nodes
(3f+1 with f = 2, matching HEIMDALLR's total fault count of 2), of which 2 are
Byzantine and silent. Each round every correct node signs the current round,
sends its accumulated signature set to f randomly chosen peers, and each
message is dropped independently with probability 0.001 (= 1 − P_norm). A node
declares a timeout when, T rounds after the fact, it still holds fewer than
2f+1 = 5 signatures for that round. One timeout at any correct node ends the
run.

T/d is the knob: PISTIS has no mechanism to detect omission faults, so its only
defence against adversarial delay is a generous timeout. Buying robustness
comparable to HEIMDALLR's costs T = 16d — and with it a worst-case recovery
delay of 3T = 48 heartbeat periods, against HEIMDALLR's just over 2.

## One parameter worth knowing about

Section VI-A says "Following [42], heartbeats in each round were sent to
X = f + 1 nodes". The simulator sends to `destination_size = f` peers and adds
the sender's own signature, so f + 1 nodes hold each heartbeat — the prose
counts the sender, the code counts recipients.

This reading is confirmed by the output: at `destination_size = f` the
simulation reproduces the published Table III columns (0.534 / 0.742 at
T = 12d, 0.000 / 0.003 at T = 8d). Sending to f + 1 *peers* would make PISTIS
more robust than the paper reports. The value is exposed as `--faulty` /
`destination_size` in `src/pistis-sim/node.h` if you want to check that
yourself.

## Cost

The PISTIS side is the expensive part: 10,000 runs × 4 timeout settings ×
2,592,000 rounds × 7 nodes. On 16 cores expect **5–6 hours**; the T = 14d and
T = 16d settings dominate because almost every run goes the full distance.
Fewer runs would separate the four settings adequately, but could not resolve
0.978 from 0.999 — that difference is smaller than the sampling error at any
lower count — so the paper's 10,000 is what this runs.

Results are cached in `results/pistis/robustness.csv`; [Figure
8](../figure-08/README.md) reuses them.

`data/reference/pistis/robustness.csv` holds the authors' published values,
transcribed from Table III — the original simulator printed prose to stdout and
no run log was kept, so it is the only reference input in the artifact that is
not raw recorded data. `./compare.py` uses it; no figure is plotted from it.

## Origin of this code

`micros/reputation/pistis/sim.cc` and `node.cc`. Changes:

- Instance count, horizon, loss rate, fault bound and the T/d list were
  compile-time constants or bare `argv[1]`; they are now command-line options
  and one invocation sweeps all four settings into a CSV.
- The original spun a busy-wait loop over an atomic counter while worker
  threads ran; replaced with a work-stealing counter and a plain join.
- The per-round signature set was an `unordered_map<round, vector<Signature>>`,
  rebuilt and re-scanned in full every round by every node. It is now a bitmask
  over node ids in a ring buffer of the last T+4 rounds. Equivalent — a
  heartbeat only ever carries rounds inside the sender's own T-round window.
- Faulty nodes now discard what they receive. They never send and never report
  a timeout, so their state cannot affect the outcome; in the original theirs
  was the one map never pruned, growing unboundedly over a 30-day horizon.

Together these are what make a 30-day horizon tractable at all. Untouched
originals are under `src/original/micros/reputation/pistis/`.
