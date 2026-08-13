# Reference data

Raw data recorded when the paper's experiments were run, on the authors'
128-core machine.

**No figure is ever produced from this directory.** `./run.sh` generates every
figure from source at the paper's parameters, writing to `results/`. This
directory exists for exactly one purpose: `./compare.py` reads it to report how
far your regenerated numbers are from the published ones.

There is deliberately no way to plot from here. An artifact with two paths to
the same filename — one measured, one recalled — is an artifact whose figures
cannot be trusted at a glance.

## `tgs/` — TGS Monte-Carlo output

One row per (α, β) grid point, from the simulator now at `src/tgs/tgs_sim.py`.
Columns: `alpha, beta, avg_survive_time, survive_rate, flag_tp, flag_fp`, where
`survive_rate` is the fraction of runs that reached the 30-day horizon and
`avg_survive_time` is the mean lifetime normalized by it.

| file | used by | P_norm (conf / delivered) | f_i | adversary | original |
|---|---|---|---|---|---|
| `result-flag-999-adaptive.txt` | Fig. 6, 7, 8, Tab. 3 | 0.999 / 0.999 | 1 | adaptive | `micros/reputation/results/` |
| `result-flag-999-aggressive.txt` | Fig. 6, 7, 8, Tab. 3 | 0.999 / 0.999 | 1 | aggressive | ″ |
| `result-flag-99-adaptive.txt` | Fig. 12 | 0.99 / 0.99 | 1 | adaptive | ″ |
| `result-flag-99-aggressive.txt` | Fig. 12 | 0.99 / 0.99 | 1 | aggressive | ″ |
| `result-flag-f2-ada.txt` | Fig. 13 | 0.99 / 0.99 | 2 | adaptive | ″ |
| `result-flag-f2-agg.txt` | Fig. 13 | 0.99 / 0.99 | 2 | aggressive | ″ |
| `result-flag-ada-dos.txt` | Fig. 14 | 0.999 / **0.99** | 1 | adaptive | ″ |
| `result-flag-agg-dos.txt` | Fig. 14 | 0.999 / **0.99** | 1 | aggressive | ″ |
| `no-tgs-p99-f1.txt` | — | 0.99 / 0.99 | 1 | aggressive, **TGS off** | `micros/reputation/result-flag-default.txt` |

`no-tgs-p99-f1.txt` is the "without TGS" baseline quoted in the Figure 12 and 14
captions (0.108 / 0.112). It is included in full rather than as the two scalars
because it demonstrates the claim that with flagging disabled the result does
not depend on (α, β) — every row agrees to within sampling noise.

The baselines for Figures 6/7 (0.143 / 0.268) and 13 (0.486 / 0.505) were not
kept as files; they are recorded as literals in `common/tgs_target.py` for
reference and are recomputed on every run.

## `pistis/robustness.csv`

Pr and normalized mean T_robust for PISTIS at T/d ∈ {8, 12, 14, 16}, from
Table III of the paper. **This is the one file here that is transcribed from the
paper rather than recorded from a run** — the original PISTIS simulator printed
prose to stdout and no log was kept. The default path regenerates these numbers
properly from `src/pistis-sim/`.

The T = 16d survive rate is stored as 0.9992 rather than the table's rounded
0.999, matching the value the original Figure 8 script used.

## `protocol-logs/` — Figure 9 raw measurements

540 per-process `.stats` files, exactly as written by the paper's benchmark
runs: `<protocol>/logs_<n>/{servers,clients}/<role>_<i>.stats`, one line of
nanoseconds per job. Copied from `protocol-comparison/*/logs_*/`.

`n` identifies the configuration: f+1 nodes for GeoShield (= HEIMDALLR), 6f+1
for the others, so `logs_7` is f_i = 1 for a 6f+1 protocol and `logs_2` is
f_i = 1 for GeoShield.

## `network/` — Table 4 totals

`ms-ptp.txt` and `heimdallr.txt`, each a Python list of microsecond totals for
f_i = 1…5, as printed by the original `analysis/*.py`. The underlying `.stats`
logs for these runs were not kept, so unlike Figure 9 only the totals survive.

## `railway/` — Figure 10 trajectories

Four ns-3 traces, `Time(ms), Location(m), Speed(m/s)` at 100 ms resolution,
renamed from `railway-ns3/track_logs/`. `./run.sh figure-10` regenerates these
by rebuilding and re-running the ns-3 scenario; because that scenario had to be
partly reconstructed (see
[figure-10's README](../../targets/figure-10/README.md#what-had-to-be-reconstructed)),
these are the most useful comparison target in this directory.

| file | original |
|---|---|
| `front-train.csv` | `train-2-slow.csv` |
| `following-stable.csv` | `train-1-slow-normal.csv` |
| `following-manual.csv` | `train-1-slow-crash.csv` |
| `following-heimdallr.csv` | `train-1.csv` |
