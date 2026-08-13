# Table 4 — Total CPU time per network measurement round

> TABLE IV: Total CPU time per network measurement round.

| f_i for both regions | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| MS-PTP (ms) | 11.30 | 30.93 | 61.28 | 102.05 |
| HEIMDALLR (ms) | 2.36 | 6.34 | 12.93 | 22.67 |

HEIMDALLR's latency-measurement module against MS-PTP, a Byzantine-resilient
timing protocol that — unlike HEIMDALLR — does not guarantee agreement on the
measured latency. Two regions run one measurement round; the table sums the CPU
time every node spends on message transmission, signature generation and
verification, timestamp processing and latency estimation.

**This is a real measurement, not a simulation.** Both protocols run as
processes on 127.0.0.1 exchanging libsodium-signed messages.

## Generate it

```bash
./run.sh table-04
```

Builds and runs both protocols on this machine, recording **100 s per
configuration** as in the paper, and writes `outputs/table-04.md`. Budget about
20 minutes.

`./compare.py network` checks the result against
`data/reference/network/{ms-ptp,heimdallr}.txt`. Note the underlying `.stats`
logs for the paper's network-measurement runs were not kept, so unlike Figure 9
only the totals survive.

Requires **libsodium** and `g++`; see the top-level README.

The driver warns if a configuration recorded too few jobs for a stable mean:

```
[heimdallr] WARNING: f=1 recorded only 30 jobs on the quietest process
(want >= 50); the mean is dominated by start-up.
```

At the paper's 100 s window this should not fire; if it does, the machine is
too loaded for the measurement to be trusted.

## What is measured

For each f_i ∈ {1, 2, 3, 4}, `src/bench.py` rebuilds with
`-DFAULTY_NODES=f`, generates keys, launches the processes, records steady
state for `--settle` seconds and averages 1,000 measurement rounds per node:

| system | source dir | nodes per region |
|---|---|---|
| MS-PTP | `src/network-meas/ms-ptp` | 3f+1 |
| HEIMDALLR | `src/network-meas/heimdallr` | f+1 |

Unlike [Figure 9](../figure-09/README.md) there is no application execution
term — a measurement round has no task to run — so the number is the protocol
cost alone.

## Reading the result

The ratio is 4.5–4.9× across the range (the paper quotes "up to 4.88×"), and it
tracks the node counts: MS-PTP needs 3f+1 measurers per region where HEIMDALLR
needs f+1, and signature verification cost grows with the number of
participants. The generated table prints the per-column ratio so you can check
this directly.

Note this is the reverse of the usual trade: HEIMDALLR provides the *stronger*
guarantee here (all correct measurers decide the same latency value, Theorem 2)
at a quarter of the cost, because agreement is reached among f+1 measurers plus
f log keepers rather than by replicating measurement across 3f+1 nodes.

## Expect different absolute numbers when you re-measure

Same caveats as Figure 9: the paper used AMD EPYC 7452 cores at 2.4 GHz with
one process per core. Per-core speed shifts both rows together and the ratio is
what the table is about. Oversubscription is much less of a concern here than
for Figure 9 — even at f_i = 4, MS-PTP needs only 13 servers + 13 clients.

## Origin of this code

`network-meas-comparison/{ms-ptp,toy-network-meas}/autorun.py` plus their
`analysis/*.py`. The `ms-ptp` copy of `autorun.py` ended by invoking
`GeoShield.py`, which does not exist in that directory, so the last step of the
pipeline had to be run by hand. Both are now driven by the shared
`src/bench.py`. The CPU-pinning fix described in
[figure-09/README.md](../figure-09/README.md#origin-of-this-code) applies here
too.

**`toy-network-meas` was renamed to `heimdallr`.** It is HEIMDALLR's own
latency-measurement protocol, not a toy — the name was a leftover from
development and would have read to a reviewer as though the comparison were
against a stripped-down stand-in. The mapping to the untouched copies:

| here | original |
|---|---|
| `src/network-meas/heimdallr` | `network-meas-comparison/toy-network-meas` |
| `src/network-meas/ms-ptp` | `network-meas-comparison/ms-ptp` |

Untouched originals keep their original names under
`src/original/network-meas-comparison/`.
