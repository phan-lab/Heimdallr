#!/usr/bin/env python3
"""Figure 10 - train positions simulating the 2011 Wenzhou incident.

Two trains 10 km apart travel at 90 m/s.  At t = 10 s the leading train loses
connectivity and enters forced braking; at t = 120 s its driver takes over in
manual (on-sight) mode at 20 km/h.  The three following-train trajectories are:

  stable network    the movement authority arrives, the train slows in time;
  manual signalling the incident case - the control centre only reaches the
                    driver at t = 120 s, too late to prevent the collision;
  with HEIMDALLR    the missing heartbeat at t = 10 s triggers a safe-mode
                    transition at t = 10.2 s, the control centre truncates the
                    movement authority and the train decelerates in time.

The trajectories come from the ns-3 ETCS-3 simulation under railway-ns3/; see
this target's README for how they were produced.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import matplotlib.pyplot as plt  # noqa: E402
import pandas as pd  # noqa: E402

import repro  # noqa: E402

MAX_TIME_S = 400   # = repro.RAILWAY_END_TIME_S


def with_start_of_run(df):
    """Prepend the t = 0 sample the track log does not contain.

    `Actuate` is first scheduled at CONNECTION_SETUP_OFFSET (5 s), so the logs
    begin at t = 5000 ms - the front train already 450 m along at 90 m/s.
    Plotting from there leaves the trace, and the crash zone drawn from it,
    floating 5 seconds clear of the axis with a bare vertical edge, instead of
    starting at the front train's initial 10 km as the published figure does.

    The original notebook meant to do this: it builds
    `init_pos = {Time(ms): 0, Location(m): 10000, Speed(m/s): 90}` but assigns
    the concatenation to `train_btr_df`, a variable it never reads again, so
    the row is discarded (railway-ns3/analysis/track-new.ipynb, cell 5).

    Back-extrapolating at the first sample's speed rather than hard-coding
    10000 m gives that same point for the leading train and 0 m for the
    following ones.
    """
    first = df.iloc[0]
    t0 = first["Time(ms)"]
    if t0 <= 0:
        return df
    start = pd.DataFrame([{
        "Time(ms)": 0.0,
        "Location(m)": first["Location(m)"] - first["Speed(m/s)"] * (t0 / 1000.0),
        "Speed(m/s)": first["Speed(m/s)"],
    }])
    return pd.concat([start, df], ignore_index=True)


def collision_time_ms(following, front):
    """First instant the following train reaches the leading train's position.

    Returns None if it never does - the manual-signalling run is supposed to end
    in a collision, so a near-miss is worth saying out loud rather than quietly
    plotting the whole trace.
    """
    merged = following.merge(front, on="Time(ms)", suffixes=("_f", "_l"))
    hit = merged[merged["Location(m)_f"] >= merged["Location(m)_l"]]
    return int(hit["Time(ms)"].iloc[0]) if not hit.empty else None


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 10")

    if not repro.ns3_available():
        raise SystemExit(
            f"ns-3 not found at {repro.ns3_root()}.\n"
            f"This figure is produced by rebuilding and running the ns-3 ETCS-3\n"
            f"scenario; there is no substitute for it.  Build ns-3.38 as described\n"
            f"in src/railway/INSTALL-ns3.md, then:\n"
            f"    NS3_ROOT=/path/to/ns-3.38 ./run.sh figure-10")
    src = repro.RESULTS / "railway"
    repro.railway_run(src, end_time=MAX_TIME_S, jobs=args.jobs, force=args.force)

    def load(name):
        return with_start_of_run(
            pd.read_csv(src / name).sort_values("Time(ms)").reset_index(drop=True))

    front = load("front-train.csv")
    stable = load("following-stable.csv")
    manual = load("following-manual.csv")
    heim = load("following-heimdallr.csv")

    front = front[front["Time(ms)"] <= MAX_TIME_S * 1000]
    # The manual-signalling run ends in a collision; the simulator keeps
    # integrating past it, so cut the trace where the following train reaches
    # the leading one.
    crash_ms = collision_time_ms(manual, front)
    if crash_ms is not None:
        manual = manual[manual["Time(ms)"] <= crash_ms]

    fig, ax = plt.subplots(figsize=(4.6, 3.1))

    ax.plot(front["Time(ms)"] / 1000, front["Location(m)"] / 1000,
            label="The Front Train\n(Stop $\\rightarrow$ On-sight)",
            color="black", ls=":", lw=2)
    # The crash zone: everything ahead of the leading train.
    #
    # linewidth=0 matters.  With an outline, fill_between draws the polygon's
    # own border - including a vertical segment up the left edge to y = 20 -
    # and if the hatch does not render (it is version- and backend-dependent
    # on a PolyCollection) that border is all you see: a red line climbing off
    # the top, easily misread as the leading train starting from a huge value.
    #
    # The translucent facecolor is a fallback for the same reason: if hatching
    # is dropped, the region is still visibly shaded rather than invisible.
    ax.fill_between(front["Time(ms)"] / 1000, front["Location(m)"] / 1000, 20,
                    facecolor=(1.0, 0.0, 0.0, 0.06), hatch="xx",
                    edgecolor="red", linewidth=0.0, zorder=0)
    ax.text(88, 16.5, "CRASH ZONE", color="black", fontsize=15, weight="bold")

    ax.plot(stable["Time(ms)"] / 1000, stable["Location(m)"] / 1000,
            color="blue", ls="-.", label="The Following Train\n(Stable Network)")
    ax.plot(manual["Time(ms)"] / 1000, manual["Location(m)"] / 1000,
            color="red", ls="--", label="The Following Train\n(Manual Signalling)")
    if crash_ms is not None:
        ax.plot(manual["Time(ms)"].to_numpy()[-1] / 1000,
                manual["Location(m)"].to_numpy()[-1] / 1000,
                color="black", marker="x", ms=10, markeredgewidth=3)
    ax.plot(heim["Time(ms)"] / 1000, heim["Location(m)"] / 1000,
            color="green", ls="-", label="The Following Train\n(with Heimdallr)")

    ax.set_ylabel("Position (km)", fontsize=14)
    ax.set_xlabel("Time (s)", fontsize=14)
    ax.set_xlim(0, MAX_TIME_S)
    ax.set_ylim(0, 20)
    ax.legend(loc="center left", fontsize=11, bbox_to_anchor=(1.02, 0.5))

    if crash_ms is None:
        gap = (front["Location(m)"].to_numpy()[-1]
               - manual["Location(m)"].to_numpy()[-1])
        print(f"  NOTE: the manual-signalling run did not collide; it stopped "
              f"{gap / 1000:.2f} km short. The published run collides at "
              f"t = 178 s. See targets/figure-10/README.md.")
    else:
        print(f"  collision at t = {crash_ms / 1000:.0f} s, "
              f"position {manual['Location(m)'].to_numpy()[-1] / 1000:.2f} km")
    print(f"  with Heimdallr the following train stops at "
          f"{heim['Location(m)'].to_numpy()[-1] / 1000:.2f} km, "
          f"front train at {front['Location(m)'].to_numpy()[-1] / 1000:.2f} km")

    repro.save(fig, "figure-10", args,
               detail=f"ns-3 scenario rebuilt and re-run, {MAX_TIME_S}s simulated")


if __name__ == "__main__":
    main()
