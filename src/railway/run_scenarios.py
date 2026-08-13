#!/usr/bin/env python3
"""Generate the Figure 10 train trajectories from the ns-3 ETCS-3 scenario.

Builds the scenario three times - once per information regime for the following
train - and runs each, collecting the resulting track logs.

  stable      the movement authority keeps arriving over a healthy network
  manual      nothing reaches the following train until the control centre
              gets word to the driver at t = 120 s (the 2011 incident)
  heimdallr   the missing heartbeat triggers a safe-mode transition at
              t = 10.2 s and the movement authority is truncated to the
              leading train's last known safe position

In all three the leading train loses connectivity at t = 10 s and its driver
takes over on-sight at t = 120 s.

Each run writes track_logs/train-<region>.csv; region 1 is the following train
and region 2 is the leading one.  The leading train's trajectory is identical
across variants, so it is taken from the stable run.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
NS3_ROOT = HERE.parent.parent.parent / "railway-ns3"

SCENARIOS = {"stable": 1, "manual": 2, "heimdallr": 3}

# The 2-train deployment: one control centre (region 0) and two trains.
# Vendored under src/railway/configs/ (copied verbatim from railway-ns3/configs/)
# so that NS3_ROOT only has to supply an ns-3 *build* - headers and libraries -
# rather than the whole 765 MB railway-ns3 tree.
CONFIG = HERE / "configs" / "config-inter-region-2trains.txt"
CONFIG_DIR = HERE / "configs" / "schedules-2trains"


def build(scenario_id: int, end_time: int, jobs: int) -> Path:
    print(f"  building SCENARIO={scenario_id} END_TIME={end_time}", flush=True)
    subprocess.run(["make", "-B", f"SCENARIO={scenario_id}",
                    f"END_TIME={end_time}", f"-j{jobs}"],
                   cwd=HERE, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return HERE / "sim"


def run(binary: Path, work: Path) -> Path:
    """Run one simulation in its own working directory; returns track_logs/."""
    shutil.rmtree(work, ignore_errors=True)
    (work / "track_logs").mkdir(parents=True)
    (work / "pcaps").mkdir(parents=True)   # the scenario enables pcap tracing

    started = time.time()
    with open(work / "run.log", "w") as log:
        proc = subprocess.run(
            [str(binary), f"--config={CONFIG}", f"--config_dir={CONFIG_DIR}"],
            cwd=work, stdout=log, stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        tail = (work / "run.log").read_text()[-800:]
        raise SystemExit(f"simulation failed ({proc.returncode}); tail of log:\n{tail}")
    print(f"    done in {time.time() - started:.0f}s", flush=True)
    return work / "track_logs"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="directory to write the four CSVs into")
    ap.add_argument("--work", default=None, help="scratch directory for the runs")
    ap.add_argument("--end-time", type=int, default=400,
                    help="seconds of simulated time (default 400, the figure's x-axis)")
    # The Makefile takes NS3_ROOT from the environment too (it uses ?=), so a
    # single `NS3_ROOT=... ./run.sh figure-10` covers both build and run.
    ap.add_argument("--ns3-root", default=os.environ.get("NS3_ROOT", str(NS3_ROOT)),
                    help="railway-ns3 checkout with a populated build/ "
                         "(default: ../../../railway-ns3, or $NS3_ROOT)")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--scenarios", default=",".join(SCENARIOS))
    args = ap.parse_args(argv)

    ns3_root = Path(args.ns3_root).resolve()
    if not (ns3_root / "build" / "include" / "ns3").is_dir():
        raise SystemExit(
            f"no prebuilt ns-3 under {ns3_root}/build.\n"
            f"This target compiles the scenario against the libraries already in "
            f"railway-ns3/build/; see targets/figure-10/README.md.")

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    work_root = Path(args.work).resolve() if args.work else out.parent / "railway-work"

    wanted = [s.strip() for s in args.scenarios.split(",") if s.strip()]
    for name in wanted:
        if name not in SCENARIOS:
            raise SystemExit(f"unknown scenario {name!r}; pick from {list(SCENARIOS)}")

    for name in wanted:
        print(f"[railway] {name}", flush=True)
        binary = build(SCENARIOS[name], args.end_time, args.jobs)
        logs = run(binary, work_root / name)

        shutil.copyfile(logs / "train-1.csv", out / f"following-{name}.csv")
        if name == "stable":
            # The leading train behaves identically in all three variants.
            shutil.copyfile(logs / "train-2.csv", out / "front-train.csv")

    print(f"\n[railway] wrote to {out}:")
    for p in sorted(out.glob("*.csv")):
        print(f"  {p.name}  ({sum(1 for _ in open(p)) - 1} samples)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
