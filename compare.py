#!/usr/bin/env python3
"""Compare the data generated on this machine against the paper's recorded runs.

    ./compare.py              everything that has been generated so far
    ./compare.py tgs railway  only those groups

Reads `results/` (what `./run.sh` produced here) and `data/reference/` (what the
authors' runs produced), and reports the difference for every quantity the
figures and tables are built from.  Nothing is regenerated; run `./run.sh`
first.

`data/reference/` is used *only* here.  No figure is ever plotted from it - the
figures come from `results/`, which `./run.sh` generates at the paper's
parameters.  This is a check, not an alternative way to produce output.

Some quantities are expected to differ and the tolerances say by how much:

  tgs       Monte-Carlo.  Each grid point is judged against its own sampling
            error, 3*sqrt(p(1-p)/N); at the paper's 10,000 runs that is about
            +-0.009 near p = 0.9.
  pistis    Monte-Carlo, same rule.
  protocol  CPU-time measurements.  Absolute values track the machine; what
            should hold is the ordering between protocols and the growth in f.
  network   as above.
  railway   trajectories from a partly reconstructed ns-3 scenario; see
            targets/figure-10/README.md.
"""

from __future__ import annotations

import argparse
import ast
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "common"))

import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

import repro  # noqa: E402
import tgs_target as T  # noqa: E402

OK, WARN, BAD = "ok", "~", "!!"


def status(delta, tol, hard):
    return OK if delta <= tol else (WARN if delta <= hard else BAD)


def sampling_tolerance(p_ref: float, n_runs: int) -> float:
    """3 sigma of the binomial sampling error at `n_runs` Monte-Carlo runs.

    Even at the paper's sample count two correct runs differ; the tolerance has
    to be the sampling error rather than a fixed number.
    """
    if not n_runs:
        return 0.05
    sigma = math.sqrt(max(p_ref * (1.0 - p_ref), 1e-9) / n_runs)
    return max(3.0 * sigma, 0.02)


def workload(path: Path) -> str:
    """The parameters behind `path`, from the sidecar `run.sh` writes."""
    meta = path.with_suffix(path.suffix + ".meta.json")
    if not meta.exists():
        return ""
    try:
        k = json.loads(meta.read_text())
    except (OSError, ValueError):
        return ""
    if k.get("kind") == "tgs":
        return f"{k['sims']} runs, {k['rounds'] // 86400}d"
    if k.get("kind") == "pistis":
        return f"{k['instances']} runs, {k['rounds'] // 86400}d"
    return ""


def n_runs(path: Path) -> int:
    """Monte-Carlo runs behind `path`, from its sidecar; 0 if unknown."""
    meta = path.with_suffix(path.suffix + ".meta.json")
    if not meta.exists():
        return 0
    try:
        k = json.loads(meta.read_text())
    except (OSError, ValueError):
        return 0
    return int(k.get("sims") or k.get("instances") or 0)


def compare_tgs() -> list:
    """Per-sweep max and mean absolute difference in the plotted quantities."""
    rows = []
    specs = [T.FIG67_ADAPTIVE, T.FIG67_AGGRESSIVE, T.FIG12_ADAPTIVE,
             T.FIG12_AGGRESSIVE, T.FIG13_ADAPTIVE, T.FIG13_AGGRESSIVE,
             T.FIG14_ADAPTIVE, T.FIG14_AGGRESSIVE]
    for spec in specs:
        gen = repro.RESULTS / "tgs" / f"{spec.key}.csv"
        ref = repro.REFERENCE / "tgs" / spec.reference
        if not gen.exists():
            rows.append(("tgs", spec.key, "-", "not generated yet", ""))
            continue
        g = repro.load_tgs(gen).set_index(["alpha", "beta"])
        r = repro.load_tgs(ref).set_index(["alpha", "beta"])
        common = g.index.intersection(r.index)
        if len(common) == 0:
            rows.append(("tgs", spec.key, BAD, "no overlapping grid points", ""))
            continue
        w, n = workload(gen), n_runs(gen)
        for col, label in (("survive_rate", "Pr"), ("avg_survive_time", "mean")):
            diff = (g.loc[common, col] - r.loc[common, col]).abs()
            # Judge each grid point against its own sampling error, then report
            # the point that exceeds it by the most.
            excess = {k: diff[k] / sampling_tolerance(r.loc[k, col], n)
                      for k in common}
            k_worst = max(excess, key=excess.get)
            rows.append(("tgs", f"{spec.key} {label}",
                         OK if excess[k_worst] <= 1 else
                         (WARN if excess[k_worst] <= 2 else BAD),
                         f"max |diff| {diff.max():.3f} at a={k_worst[0]:g},b={k_worst[1]:g} "
                         f"({excess[k_worst]:.1f}x tol)",
                         f"{len(common)} pts" + (f", {w}" if w else "")))
    return rows


def compare_pistis() -> list:
    gen = repro.RESULTS / "pistis" / "robustness.csv"
    ref = repro.REFERENCE / "pistis" / "robustness.csv"
    if not gen.exists():
        return [("pistis", "robustness", "-", "not generated yet", "")]
    g = pd.read_csv(gen).set_index("timeout_rounds")
    r = pd.read_csv(ref).set_index("timeout_rounds")
    rows = []
    for t in r.index.intersection(g.index):
        d = abs(g.loc[t, "survive_rate"] - r.loc[t, "survive_rate"])
        w, n = workload(gen), n_runs(gen)
        tol = sampling_tolerance(r.loc[t, "survive_rate"], n)
        rows.append(("pistis", f"T = {t}d",
                     OK if d <= tol else (WARN if d <= 2 * tol else BAD),
                     f"Pr {g.loc[t, 'survive_rate']:.3f} vs {r.loc[t, 'survive_rate']:.3f}",
                     w or f"{int(g.loc[t, 'instances'])} runs"))
    return rows


def compare_protocol() -> list:
    sys.path.insert(0, str(repro.SRC))
    import bench  # noqa: E402

    gen_root = repro.RESULTS / "protocol-logs"
    ref_root = repro.REFERENCE / "protocol-logs"
    if not gen_root.is_dir():
        return [("protocol", "cpu time", "-", "not generated yet", "")]

    rows = []
    for name in bench.FIGURE9_ORDER:
        proto = bench.PROTOCOLS[name]
        ratios = []
        for f in [1, 2, 3, 4]:
            try:
                g = bench.overhead_us(gen_root, proto, f)
                r = bench.overhead_us(ref_root, proto, f)
            except FileNotFoundError:
                continue
            ratios.append(g / r)
        if not ratios:
            rows.append(("protocol", proto.label, "-", "no overlapping runs", ""))
            continue
        # Machine speed shifts everything; what matters is that it shifts
        # everything by a similar factor and stays monotonic in f.
        spread = max(ratios) / min(ratios)
        rows.append(("protocol", proto.label,
                     status(spread, 2.0, 4.0),
                     f"this machine / paper = {min(ratios):.2f}-{max(ratios):.2f}x",
                     f"{len(ratios)} fault levels"))
    return rows


def compare_network() -> list:
    sys.path.insert(0, str(repro.SRC))
    import bench  # noqa: E402

    gen_root = repro.RESULTS / "network-logs"
    if not gen_root.is_dir():
        return [("network", "cpu time", "-", "not generated yet", "")]

    rows = []
    for label, key, rel in (("MS-PTP", "ms-ptp", "network/ms-ptp.txt"),
                            ("HEIMDALLR", "heimdallr", "network/heimdallr.txt")):
        ref = ast.literal_eval((repro.REFERENCE / rel).read_text().strip())
        proto = bench.PROTOCOLS[key]
        ratios = []
        for i, f in enumerate([1, 2, 3, 4]):
            try:
                g = bench.overhead_us(gen_root, proto, f)
            except FileNotFoundError:
                continue
            ratios.append(g / ref[i])
        if not ratios:
            rows.append(("network", label, "-", "no overlapping runs", ""))
            continue
        spread = max(ratios) / min(ratios)
        rows.append(("network", label,
                     status(spread, 2.0, 4.0),
                     f"this machine / paper = {min(ratios):.2f}-{max(ratios):.2f}x",
                     f"{len(ratios)} fault levels"))
    return rows


def compare_railway() -> list:
    gen_dir = repro.RESULTS / "railway"
    ref_dir = repro.REFERENCE / "railway"
    if not gen_dir.is_dir():
        return [("railway", "trajectories", "-", "not generated yet", "")]

    rows = []
    for name in ("front-train", "following-stable", "following-manual",
                 "following-heimdallr"):
        gp, rp = gen_dir / f"{name}.csv", ref_dir / f"{name}.csv"
        if not gp.exists():
            rows.append(("railway", name, "-", "not generated yet", ""))
            continue
        g = pd.read_csv(gp).sort_values("Time(ms)")
        r = pd.read_csv(rp).sort_values("Time(ms)")
        # Compare position at matching timestamps.
        m = g.merge(r, on="Time(ms)", suffixes=("_g", "_r"))
        if m.empty:
            rows.append(("railway", name, BAD, "no overlapping timestamps", ""))
            continue
        d = (m["Location(m)_g"] - m["Location(m)_r"]).abs()
        early = m[m["Time(ms)"] <= 120_000]
        d_early = (early["Location(m)_g"] - early["Location(m)_r"]).abs()
        rows.append(("railway", name,
                     status(d.max(), 500, 2000),
                     f"max |Δpos| {d.max():.0f} m over the run, "
                     f"{d_early.max():.0f} m before t=120 s",
                     f"{len(m)} samples"))
    return rows


GROUPS = {"tgs": compare_tgs, "pistis": compare_pistis,
          "protocol": compare_protocol, "network": compare_network,
          "railway": compare_railway}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("groups", nargs="*", default=None,
                    help=f"which groups to compare (default all): {', '.join(GROUPS)}")
    args = ap.parse_args(argv)

    wanted = args.groups or list(GROUPS)
    for g in wanted:
        if g not in GROUPS:
            raise SystemExit(f"unknown group {g!r}; pick from {list(GROUPS)}")

    rows = []
    for g in wanted:
        rows.extend(GROUPS[g]())

    if not rows:
        print("nothing to compare")
        return 0

    w1 = max(len(r[1]) for r in rows) + 2
    w2 = max(len(r[3]) for r in rows) + 2
    print(f"{'':4}{'quantity':<{w1}}{'comparison':<{w2}}note")
    print("-" * (4 + w1 + w2 + 20))
    for group, name, mark, detail, note in rows:
        print(f"{mark:<4}{name:<{w1}}{detail:<{w2}}{note}")

    bad = sum(1 for r in rows if r[2] == BAD)
    missing = sum(1 for r in rows if r[2] == "-")
    print()

    print(f"{sum(1 for r in rows if r[2] == OK)} within tolerance, "
          f"{sum(1 for r in rows if r[2] == WARN)} marginal, "
          f"{bad} out of tolerance, {missing} not generated yet")
    if missing:
        print("Run ./run.sh all to generate the missing pieces.")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
