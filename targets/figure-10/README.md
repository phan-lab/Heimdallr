# Figure 10 — Trains' positions simulating the Wenzhou incident

> Fig. 10: Trains' positions simulating the Wenzhou incident.

The 2011 Wenzhou collision, replayed against an ETCS-3 railway control system
in ns-3. Two trains start 10 km apart at 90 m/s. At t = 10 s the leading train
loses connectivity and enters forced braking; at t = 120 s its driver takes
over in manual (on-sight) mode at 20 km/h. Three trajectories for the following
train are overlaid:

- **stable network** (blue) — the movement authority keeps arriving, the train
  slows in time;
- **manual signalling** (red) — the incident: nothing reaches the following
  train until the control centre gets word to the driver at t = 120 s, too late;
  the trace ends at the collision;
- **with HEIMDALLR** (green) — the heartbeat expected at t = 10 s never
  arrives, the control centre transitions to safe mode at t = 10.2 s and
  truncates the movement authority to the leading train's last known safe
  position, so the following train decelerates and stops clear.

## Generate it

```bash
NS3_ROOT=/opt/ns-3.38 ./run.sh figure-10
```

Compiles the ns-3 scenario three times — once per variant — runs each for 400 s
of simulated time, and plots the resulting traces. **Budget about 16 minutes**:
roughly 5 minutes per scenario, the LTE model being the expensive part, plus a
20-second rebuild each. Results are cached in `results/railway/`, so re-plotting
is instant afterwards (`--force` to redo).

Without ns-3 this target **stops with instructions** rather than substituting
anything; see [Getting ns-3](#getting-ns-3) below.

## Getting ns-3

This is the only target that needs software outside this repository. See
**[src/railway/INSTALL-ns3.md](../../src/railway/INSTALL-ns3.md)** — it has a
Dockerfile and a manual recipe. The parts that are easy to get wrong:

- **ns-3.38 specifically** — the libraries are versioned in their filenames.
- **C++17, so GCC 11 or 12** — ns-3.38's `Singleton<T>` does not compile as
  C++20, and GCC 13+ needs `<cstdint>` includes it predates.
- **libpbc** — the `multisig` contrib module links it, and it is not in the
  Debian/Ubuntu archive; it must be built from source.

The scenario source, the configs and the four HEIMDALLR contrib modules
(`multisig`, `seconomist-message`, `secure-log`, `task-schedules`) are vendored
under `src/railway/`, so `NS3_ROOT` only has to supply an ns-3 build.

## How it is built

ns-3's own build system cannot be used here: `railway-ns3/cmake-cache/CMakeCache.txt`
records absolute paths from the machine the tree was built on
(`/home/caiyifan/version-controlled/seconomist-simulation`), so `./ns3 configure`
would discard the prebuilt libraries and rebuild all of ns-3 — modules,
dependencies and all — from source.

Instead `src/railway/Makefile` compiles the scenario directly against the
libraries and headers already present in `railway-ns3/build/`:

```bash
cd src/railway
make SCENARIO=3 END_TIME=400        # ~20 s
./sim --config=configs/config-inter-region-2trains.txt \
      --config_dir=configs/schedules-2trains
```

Two details worth knowing if you adapt this:

- **`-std=c++17` is required.** ns-3.38's `Singleton<T>` declares its deleted
  copy constructor as `Singleton<T>(const Singleton<T>&)`, which is ill-formed
  in C++20.
- **The working directory needs `track_logs/` and `pcaps/`** to exist before
  the run, or ns-3 aborts on the first trace write. `run_scenarios.py` creates
  both.

Set `NS3_ROOT` to wherever you built ns-3.

## What had to be reconstructed

The shipped tree does not contain this scenario in runnable form. Settings were
compiled out, commented out, or disagree with the paper, and were toggled by
hand between the paper's runs:

| what | shipped state | needed for Fig. 10 |
|---|---|---|
| `ENABLE_TRACK_LOG` (`defs.h`) | `0` — no CSV is written at all | `1` |
| `END_TIME` (`defs.h`) | `60` s | `400` s (the figure's x-axis) |
| `PING_TIMEOUT` (`defs.h`) | `500000` µs = 500 ms | `200000` µs — **the paper's D_t/o** |
| leading-train behaviour (`train_app.cc`) | commented out | forced braking at t = 10 s, on-sight 5.56 m/s at t = 120 s |
| following-train information regime | commented out, one block per variant | selected by `FIG10_SCENARIO` |
| initial placement (`train_app.cc`) | `region_id * 2000 m`, at rest | 10 km apart, both at `MAX_SPEED` |
| "no authority yet" sentinel | `ma_location = 0` — reads as a restriction *behind* the train | `-1`, the code's own unrestricted case |

The fault injection is **recovered**, not invented: the commented block

```c
// if (node_info.region_id == 2 && Simulator::Now().GetSeconds() >= 10)
//     target_speed = emergency_stop;
// if (node_info.region_id == 2 && Simulator::Now().GetSeconds() >= 120)
//     target_speed = 5.56;
```

is exactly the paper's "loses connectivity at t = 10.0 s … at t = 120.0 s the
driver takes over and moves the train at 20 km/h" (5.56 m/s = 20 km/h).

The initial placement is **inferred**. The recorded traces begin at t = 5000 ms
— which is `CONNECTION_SETUP_OFFSET`, when `Actuate` is first scheduled — with
the following train at 450 m and the leading one at 10450 m. At 90 m/s, 5 s of
travel is 450 m, so the two started at 0 m and 10000 m already at line speed.
That is what `FIG10_SCENARIO` sets up.

**These are edits to the simulation source**, made from the paper's description
and the commented-out remnants. `./compare.py railway` reports the deviation of
each generated trace from the authors' recorded one:

| trace | max |Δpos| | before t = 120 s |
|---|---|---|
| front train | **0 m** | 0 m |
| following, stable network | 13 m | 9 m |
| following, manual signalling | 535 m | **0 m** |
| following, with HEIMDALLR | 1215 m | **0 m** |

The leading train and both following trains are exact up to the point where
their scenarios branch. Afterwards the manual run collides at t = 176 s against
the published 178 s. The HEIMDALLR run stops at 9.95 km where the published one
stops at 11.47 km — both comfortably clear of the leading train at 15.8 km, so
the safety outcome is the same, but the resting position differs: the movement
authority is truncated to the leading train's position at the moment
connectivity was lost (10.9 km), and the published run evidently used a
slightly later or more permissive bound. That one is not recoverable from the
tree, so it is left as-is rather than tuned to match.

Everything is behind `#if FIG10_SCENARIO`, so `SCENARIO=0` (the default) leaves
the tree's behaviour untouched. Diffing `src/railway/simulation/` against
`src/original/railway-ns3/scratch/simulation/` shows every change.

### The timeout disagreed with the paper

`PING_TIMEOUT` is D_t/o, the inter-region timeout. Section VII-A states:

> We set the heartbeat interval R_hb = 1 s and timeout D_t/o = 200 ms, to
> ensure the recovery time (by Theorem 5) is within ETCS timing specification.

The tree ships `PING_TIMEOUT (500000)` — 500 ms. The heartbeat interval does
match (`PING_INTERVAL (1000000)` = 1 s), so only the timeout is off. It also has
to be 200 ms for the figure's own narrative to hold: a heartbeat due at
t = 10.0 s that never arrives triggers safe mode at t = 10.2 s, which is
10.0 s + D_t/o. At the shipped 500 ms it would be t = 10.5 s.

The Makefile overrides it to the paper's value, and the safe-mode trigger is
now derived from `PING_TIMEOUT` rather than hard-coded, so the two cannot drift
apart again:

```c
if (Simulator::Now().GetMicroSeconds() >= 10 * 1000000 + PING_TIMEOUT)
    front_loc = FIG10_LAST_KNOWN;
```

Override it back with `make PING_TIMEOUT=500000` to see the shipped behaviour.

### The driver's reaction delay, recovered from the trace

The manual-signalling run is supposed to end in a collision. With the movement
authority applied at t = 120 s — when the paper says the control centre reaches
the driver — the following train brakes early enough to stop 1.6 km short, and
there is no collision.

The recorded trace says why. It holds 90 m/s through **t = 130 s** and only then
decelerates at `MAX_EMERGENT_ACC`. Taking that onset from its speed at
t = 150 s and predicting t = 178 s gives 14637.6 m at 32.40 m/s, against
**14638 m at 32.40 m/s** recorded — position and speed both to the decimal, and
predicted rather than fitted. So the driver takes 10 s to apply the brake after
the t = 120 s call.

With `FIG10_DRIVER_REACTION_S = 10`, the collision lands at t = 176 s and
14.56 km, against the published t = 178 s and 14.64 km.

### Every train braked five seconds early

Comparing the regenerated traces against the recorded ones caught this, twice.
At t = 10 s the recording has both trains at 90.0 m/s; the regenerated runs had
them at 84.1 m/s and already decelerating.

`ma_location` is initialised to `0`, and `CalcTargetSpeed` reads a movement
authority ending at 0 m as one the train has already passed — so on the first
`Actuate` tick (t = 5 s, `CONNECTION_SETUP_OFFSET`) every train commands an
emergency stop, until a real authority arrives. The code's own "unrestricted"
case is `front_loc < 0`, so the sentinel should have been negative.

It never surfaced in the shipped configuration because there the trains start
stationary 2 km apart, where an authority at 0 m changes nothing. `-1` under
`FIG10_SCENARIO` fixes it for both trains at once; the recorded traces hold a
flat 90 m/s through t = 15 s, and so do the regenerated ones.

This is the clearest argument for `./compare.py` existing: the figure looked
right with the bug in place. Only the numeric comparison against the recorded
traces showed 84.1 m/s where the paper has 90.0.

## Is this the latest version of the scenario?

Yes. Every copy of `scratch/simulation/` on this machine —
`seconomist-simulation`, `GeoShield-eval`, `Heimdallr-artifact`,
`GeoShield-artifact-usenix` and this one — is byte-identical
(`train_app.cc` md5 `68dbc99b`, `defs.h` md5 `ecd40625`), with the oldest
dating from 2023. There is no newer revision to pick up; the discrepancies
above are between the code as it exists and the paper's description of it.

## Reading the result

The manual-signalling trace is cut where the following train's position first
reaches the leading train's — the collision. The simulator keeps integrating
past that point, which is why the original notebook truncated it at a
hard-coded 178000 ms; here the crossing is computed from the two traces.

## Origin of the plotting code

`railway-ns3/analysis/track-new.ipynb`, cell 6 — a notebook cell with absolute
paths into `../track_logs/`, that hard-coded truncation, and the pre-rename
"GeoShield" legend.
