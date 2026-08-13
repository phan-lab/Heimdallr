# Figure 9 — Total CPU time per inter-region job

> Fig. 9: Total CPU time per inter-region job under HEIMDALLR and existing BFT protocols.

Two panels (task execution time 200 µs and 1 ms) comparing HEIMDALLR against
PBFT, Zyzzyva (ideal and omission paths) and PISTIS at T = 8d and T = 16d, as
the maximum number of faulty nodes in the system grows from 2 to 8.

**This is a real measurement, not a simulation.** Every protocol runs as a set
of processes on 127.0.0.1 exchanging signed messages through libsodium.

## Generate it

```bash
./run.sh figure-09
```

Rebuilds each protocol per fault bound, launches it, records **100 s of steady
state** (200 s for PISTIS, as in the paper) and analyses the resulting `.stats`
logs into `results/protocol-logs/`. **Budget about 1.1 hours.** Requires
**libsodium** (`-lsodium`) and `g++`.

Afterwards, `./compare.py protocol` compares against the 540 raw per-process
`.stats` files the authors recorded, preserved under
`data/reference/protocol-logs/`.

## What is measured

For each protocol and each f_i ∈ {1, 2, 3, 4} — plotted as F = 2 f_i, the
system-wide fault bound across the two regions — `src/bench.py`:

1. rebuilds with `-DFAULTY_NODES=<f or 2f>`,
2. generates key pairs (`keygen/`),
3. launches n server and n client processes pinned across the available cores,
4. records steady state for `--settle` seconds, then stops everything.

Each process accumulates the CPU time it spends on protocol work per job with
a `Profiler` and writes one nanosecond figure per job to stderr, which the run
scripts capture as `logs_<n>/{servers,clients}/*.stats`. The plotted total is

```
sum over all processes of (mean ns per job) / 1000   +   exec_time_us × replicas
```

The second term is the application work the replication factor forces the
system to repeat: f_i+1 replicas for HEIMDALLR against 3f+1 per region (6f+1
across two regions) for the BFT baselines. That is where most of the gap comes
from, and why it widens with f.

| protocol | source dir | nodes | replicas |
|---|---|---|---|
| Heimdallr | `src/protocol-comparison/GeoShield` | f+1 | f+1 |
| PBFT | `src/protocol-comparison/PBFT` | 6f+1 | 6f+1 |
| Zyzzyva-ideal | `src/protocol-comparison/Zyzzyva` | 6f+1 | 6f+1 |
| Zyzzyva-omission | `src/protocol-comparison/Zyzzyva-fault` | 6f+1 | 6f+1 |
| PISTIS (T = 8d) | `src/protocol-comparison/PISTIS` | 6f+1, servers only | 6f+1 |
| PISTIS (T = 16d) | `src/protocol-comparison/PISTIS-16d` | 6f+1, servers only | 6f+1 |

HEIMDALLR is still named `GeoShield` in the source tree.

## Expect different absolute numbers when you re-measure

The paper's runs used AMD EPYC 7452 cores at 2.4 GHz with one process per core.
Two things change on a workstation:

- **Per-core speed.** On a faster core every protocol shifts down together; the
  ratios between protocols are what the figure is about and they hold.
- **Oversubscription.** The BFT baselines drive one replica group from a
  *single* client, so the process count is n + 1, not 2n:

  | f_i | F = 2f_i | replicas (6f_i+1) | processes |
  |---|---|---|---|
  | 1 | 2 | 7 | 7 + 1 = 8 |
  | 2 | 4 | 13 | 13 + 1 = 14 |
  | 3 | 6 | 19 | 19 + 1 = 20 |
  | 4 | 8 | 25 | 25 + 1 = **26** |

  PISTIS has no downstream region, so it is n exactly (25 at f_i = 4).
  HEIMDALLR is the only one with n clients — but its n is f_i + 1, so 5 + 5 = 10.
  Below ~26 cores the pinning wraps (`i % nproc`) and processes share cores,
  which inflates the measured CPU time, most for the protocols using the most
  processes.

If you want numbers comparable to the paper, run on a machine with ≥ 26 cores,
or restrict to `--f-values 1,2`, whose largest configuration is 14 processes.

## PBFT and message reordering

The PBFT baseline required job ids to arrive strictly in sequence and threw
`std::runtime_error("Invalid job id ...")` otherwise, killing the replica. On
the paper's 128-core machine that never fires: one process per core, and
loopback datagrams arrive in order. With 25–31 processes on 16 cores a PREPARE
can overtake its PRE-PREPARE, and the original aborted — losing every PBFT
configuration above f = 1.

`upstream.cc` now resynchronises onto the primary's job id instead of aborting,
and ignores PREPARE/COMMIT messages for jobs it has moved past. The replica
still performs the full per-job signature verification, which is what is being
measured. Each process counts the events and reports them in its `.log`:

```
# resynchronised 1 time(s) (job 4 after 3)
```

**A nonzero count means the host was too oversubscribed for that measurement to
be trusted.** Check the logs before quoting a number.

## Origin of this code

Six copies of `autorun.py` plus six copies of `analysis/<Protocol>.py`, one per
protocol directory, differing only in the node-count formula and the sleep
duration — and drifting (the `ms-ptp` copy still invoked `GeoShield.py`). They
are replaced by a single `src/bench.py` with a table of protocol descriptors.
The run scripts also pinned servers to CPUs 63, 62, … which fails outright on
any machine with fewer cores; the copies under `src/` pin modulo `nproc`.
Untouched originals are under `src/original/protocol-comparison/`.
