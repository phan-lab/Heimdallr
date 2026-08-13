#!/usr/bin/env python3
"""Driver and analysis for the localhost BFT/measurement benchmarks.

Replaces the six near-identical ``autorun.py`` + ``analysis/<Protocol>.py``
pairs in the original tree, where the fault range, the run duration and the
node-count formula were duplicated (and drifted) between copies.

Every protocol runs entirely on 127.0.0.1: `n` server processes and, for the
protocols that have a downstream region, `n` client processes.  Each process
accumulates the CPU time it spends on protocol work with a Profiler and prints
one nanosecond figure per job to stderr, which the run scripts redirect into
``logs_<n>/{servers,clients}/*.stats``.

The reported total is, per inter-region job:

    sum over all processes of (mean ns per job) / 1000   +   exec_us * replicas

i.e. protocol overhead plus the application task execution that the replication
factor forces the system to repeat.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent

# Task execution times swept per configuration, in microseconds.  Figure 9
# plots columns 1 (200 us) and 3 (1 ms).
EXEC_TIME_US = [100, 200, 500, 1000]


@dataclass
class Protocol:
    """How one protocol is built, launched and accounted for."""
    path: str                # source directory, relative to src/
    label: str               # name used in the paper's figures
    nodes: str               # node-count formula: 'f+1', '3f+1' or '6f+1'
    define: str = "f"        # value of the FAULTY_NODES compile-time define
    servers_only: bool = False   # PISTIS broadcasts; there is no downstream region
    settle_s: int = 100          # seconds of steady state to record
    f_values: list = field(default_factory=lambda: [1, 2, 3, 4, 5])

    @property
    def dir(self) -> str:
        return self.path.split("/")[-1]

    @property
    def src(self) -> Path:
        return HERE / self.path

    def num_nodes(self, f: int) -> int:
        return {"f+1": f + 1, "3f+1": 3 * f + 1, "6f+1": 6 * f + 1}[self.nodes]

    def faulty_define(self, f: int) -> int:
        return f if self.define == "f" else 2 * f

    def replicas(self, f: int) -> int:
        """Task replicas the protocol runs per inter-region job."""
        return self.num_nodes(f)


PROTOCOLS = {
    # --- Figure 9: BFT protocol comparison ---------------------------------
    # HEIMDALLR is still called GeoShield in the source tree.
    "GeoShield":    Protocol("protocol-comparison/GeoShield", "Heimdallr", "f+1"),
    "PBFT":         Protocol("protocol-comparison/PBFT", "PBFT", "6f+1", define="2f"),
    "Zyzzyva":      Protocol("protocol-comparison/Zyzzyva", "Zyzzyva-ideal", "6f+1",
                             define="2f", f_values=[1, 2, 3, 4]),
    "Zyzzyva-fault": Protocol("protocol-comparison/Zyzzyva-fault", "Zyzzyva-omission",
                              "6f+1", define="2f", f_values=[1, 2, 3, 4]),
    "PISTIS":       Protocol("protocol-comparison/PISTIS", r"PISTIS ($\mathbb{T}=8d$)",
                             "6f+1", define="2f", servers_only=True, settle_s=200),
    "PISTIS-16d":   Protocol("protocol-comparison/PISTIS-16d", r"PISTIS ($\mathbb{T}=16d$)",
                             "6f+1", define="2f", servers_only=True, settle_s=200),
    # --- Table 4: network latency measurement ------------------------------
    "ms-ptp":       Protocol("network-meas/ms-ptp", "MS-PTP", "3f+1",
                             f_values=[1, 2, 3, 4, 5]),
    "heimdallr":    Protocol("network-meas/heimdallr", "Heimdallr", "f+1",
                             f_values=[1, 2, 3, 4, 5]),
}

# Order used in Figure 9's legend.
FIGURE9_ORDER = ["PISTIS-16d", "PISTIS", "PBFT", "Zyzzyva-fault", "Zyzzyva", "GeoShield"]
# Rows of Table 4.
TABLE4_ORDER = ["ms-ptp", "heimdallr"]


# --------------------------------------------------------------------------
# Analysis
# --------------------------------------------------------------------------
# A configuration recorded for too few jobs gives a mean dominated by process
# start-up; warn rather than silently reporting a number that is off by 10x.
MIN_SAMPLES = 50


def mean_per_file(directory: Path) -> dict:
    """{filename: (mean ns per job, number of samples)} for every .stats file."""
    out = {}
    for path in sorted(glob.glob(str(directory / "*.stats"))):
        values = []
        with open(path) as fh:
            for line in fh:
                try:
                    values.append(float(line.strip()))
                except ValueError:
                    continue
        if values:
            out[os.path.basename(path)] = (sum(values) / len(values), len(values))
    return out


def overhead_us(log_root: Path, proto: Protocol, f: int) -> float:
    """Protocol CPU time per job, in microseconds, summed over all processes."""
    logs = log_root / proto.dir / f"logs_{proto.num_nodes(f)}"
    if not logs.is_dir():
        raise FileNotFoundError(logs)
    stats = dict(mean_per_file(logs / "servers"))
    stats.update(mean_per_file(logs / "clients"))
    if not stats:
        # The processes wrote something other than timings - almost always
        # "Bind failed" from a previous run still holding the ports.  Surface it
        # instead of an opaque "no samples".
        detail = ""
        for path in sorted(glob.glob(str(logs / "*" / "*.stats"))):
            first = open(path).readline().strip()
            if first:
                detail = f": {os.path.basename(path)} says {first!r}"
                break
        raise FileNotFoundError(f"{logs} contains no timing samples{detail}")

    fewest = min(n for _, n in stats.values())
    if fewest < MIN_SAMPLES:
        print(f"  [{proto.dir}] WARNING: f={f} recorded only {fewest} jobs on the "
              f"quietest process (want >= {MIN_SAMPLES}); the mean is dominated by "
              f"start-up. Re-run with a longer --settle.", file=sys.stderr)
    return sum(mean for mean, _ in stats.values()) / 1000.0


def analyse(log_root: Path, protocols=None, f_values=None) -> dict:
    """{protocol: {'label':..., 'f': [...], 'total_us': [[per exec time], ...]}}

    ``total_us[i][j]`` is the CPU time per job at f = f[i] when the application
    task takes EXEC_TIME_US[j].  Column 0 of the measurement-only protocols
    (Table 4) is the protocol cost alone, since exec time is not part of a
    measurement round; use ``overhead_us`` directly for those.
    """
    results = {}
    for name in (protocols or FIGURE9_ORDER):
        proto = PROTOCOLS[name]
        fs, rows = [], []
        for f in (f_values or proto.f_values):
            try:
                base = overhead_us(log_root, proto, f)
            except FileNotFoundError as exc:
                print(f"  [{name}] no data for f={f} ({exc}); skipping", file=sys.stderr)
                continue
            fs.append(f)
            rows.append([base + e * proto.replicas(f) for e in EXEC_TIME_US])
        if fs:
            results[name] = {"label": proto.label, "f": fs,
                             "exec_time_us": EXEC_TIME_US, "total_us": rows}
    return results


# --------------------------------------------------------------------------
# Running
# --------------------------------------------------------------------------
def wait_for_ports(timeout_s: int = 15):
    """Block until the previous run's processes are actually gone.

    stop-all.sh only sends the signal; launching again before the old
    processes have released the ports makes every new process die with
    "Bind failed" and produce an empty .stats file.
    """
    patterns = ["upstream [0-9]", "downstream [0-9]", "grand-master [0-9]",
                "follower [0-9]"]
    for _ in range(timeout_s * 4):
        alive = any(subprocess.run(["pgrep", "-f", p], stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL).returncode == 0
                    for p in patterns)
        if not alive:
            return
        time.sleep(0.25)
    print("  WARNING: processes from a previous run are still alive; the next "
          "launch may fail to bind. Is another benchmark running?",
          file=sys.stderr)


def sh(cmd, cwd, check=True):
    """Run a shell command in `cwd`, quiet unless it fails."""
    print(f"    $ {cmd}", flush=True)
    r = subprocess.run(cmd, cwd=str(cwd), shell=True, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        # The launch scripts are chatty on success; only show their output when
        # something went wrong, where it is the only clue available.
        output = (r.stdout or "").strip()
        if output:
            for line in output.splitlines()[-20:]:
                print(f"      | {line}", file=sys.stderr)
        if check:
            raise SystemExit(f"command failed ({r.returncode}) in {cwd}: {cmd}")
    return r.returncode


def run_protocol(name: str, log_root: Path, settle_s: int | None, f_values=None):
    proto = PROTOCOLS[name]
    src = proto.src
    settle = settle_s if settle_s is not None else proto.settle_s

    for f in (f_values or proto.f_values):
        n = proto.num_nodes(f)
        print(f"  [{name}] f={f}  nodes={n}  recording {settle}s", flush=True)
        started = time.time()

        sh("./stop-all.sh", src, check=False)
        wait_for_ports()
        # Rebuild first: all.sh runs a plain `make`, which would not pick up a
        # changed FAULTY_NODES on its own.
        sh(f"make -B -s CXXFLAGS='-std=c++17 -O3 -DFAULTY_NODES={proto.faulty_define(f)}'", src)
        shutil.rmtree(src / f"logs_{n}", ignore_errors=True)

        # Each protocol's own all.sh knows how to launch it - the run scripts
        # take different argument counts and the BFT protocols drive their
        # replica group from a single client.  PISTIS is the exception: it has
        # no downstream region, and its all.sh calls a run-clients.sh that does
        # not exist, so launch its servers directly as the original autorun did.
        if proto.servers_only:
            sh(f"cd keygen && ./keygen {n}", src)
            sh(f"./run-servers.sh {n}", src)
        else:
            sh(f"./all.sh {n}", src)

        time.sleep(settle)
        sh("./stop-all.sh", src, check=False)
        time.sleep(2)

        dest = log_root / proto.dir / f"logs_{n}"
        shutil.rmtree(dest, ignore_errors=True)
        dest.parent.mkdir(parents=True, exist_ok=True)
        if (src / f"logs_{n}").is_dir():
            shutil.move(str(src / f"logs_{n}"), str(dest))
            print(f"    -> {dest}  ({time.time() - started:.0f}s incl. rebuild)",
                  flush=True)
        else:
            print(f"    !! no logs produced for f={f}", file=sys.stderr)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log-root", required=True,
                    help="directory the per-protocol logs_<n> trees are written to")
    ap.add_argument("--protocols", default=",".join(FIGURE9_ORDER))
    ap.add_argument("--f-values", default=None,
                    help="comma separated f_i values (default: each protocol's own range)")
    ap.add_argument("--settle", type=int, default=None,
                    help="seconds of steady state to record (default: 100, PISTIS 200)")
    ap.add_argument("--analyse-only", action="store_true",
                    help="skip the runs and just recompute from existing logs")
    ap.add_argument("--json", default=None, help="write the analysis to this path")
    args = ap.parse_args(argv)

    log_root = Path(args.log_root).resolve()
    log_root.mkdir(parents=True, exist_ok=True)
    names = [p.strip() for p in args.protocols.split(",") if p.strip()]
    f_values = [int(x) for x in args.f_values.split(",")] if args.f_values else None

    if not args.analyse_only:
        for name in names:
            print(f"[bench] {name}", flush=True)
            run_protocol(name, log_root, args.settle, f_values)

    results = analyse(log_root, names, f_values)
    text = json.dumps(results, indent=2)
    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json).write_text(text)
        print(f"[bench] wrote {args.json}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
