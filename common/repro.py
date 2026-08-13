"""Shared helpers for the HEIMDALLR artifact reproduction scripts.

Every ``targets/<name>/generate.py`` imports this module.  It centralises:
  * where things live,
  * the experiment parameters, which are the paper's and are not configurable,
  * the matplotlib defaults the paper figures were produced with,
  * the TGS heat-map panel used by Figures 6, 7, 12, 13 and 14.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = ROOT / "data" / "reference"
RESULTS = ROOT / "results"
OUTPUTS = ROOT / "outputs"
SRC = ROOT / "src"

# Fonts embedded as TrueType, matching the paper's rcParams.
matplotlib.rcParams["pdf.fonttype"] = 42
matplotlib.rcParams["ps.fonttype"] = 42
matplotlib.rcParams["savefig.bbox"] = "tight"
matplotlib.rcParams["font.family"] = "sans-serif"
matplotlib.rcParams["font.size"] = 12
matplotlib.rcParams["axes.labelsize"] = 14
matplotlib.rcParams["xtick.labelsize"] = 13
matplotlib.rcParams["ytick.labelsize"] = 13
matplotlib.rcParams["legend.fontsize"] = 12
# Hatch line width defaults have changed between matplotlib releases and some
# builds render hatching on a PolyCollection faintly or not at all.  Set it
# explicitly so the crash zone in Figure 10 looks the same everywhere.
matplotlib.rcParams["hatch.linewidth"] = 1.0


# --------------------------------------------------------------------------
# Experiment parameters - the paper's, and only the paper's
# --------------------------------------------------------------------------
# There is one configuration and it is the one described in the paper.  There
# used to be reduced "profiles" and a mode that replotted the recorded data;
# both are gone, because a figure produced at a reduced sample count is not the
# paper's figure and having two ways to produce the same filename invites
# exactly the confusion this artifact exists to avoid.
#
# Sec. VI-A: 10,000 simulations per (alpha, beta) over a 30-day horizon.
# Sec. VI-B: 1,000 runs per protocol configuration.
HORIZON_ROUNDS = 86_400 * 30   # 30 days at one heartbeat per second
TGS_RUNS = 10_000              # Monte-Carlo runs per (alpha, beta) grid point
PISTIS_RUNS = 10_000           # runs per T/d setting
SETTLE_S = 100                 # seconds recorded per benchmark configuration
SETTLE_S_PISTIS = 200          # PISTIS needs longer to reach steady state
RAILWAY_END_TIME_S = 400       # simulated seconds for the railway scenario

TGS_COLUMNS = ["alpha", "beta", "avg_survive_time", "survive_rate", "flag_tp", "flag_fp"]


def make_parser(description: str) -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1,
                    help="parallel workers (default: all cores). Affects speed "
                         "only, never results.")
    ap.add_argument("--force", action="store_true",
                    help="recompute even if a cached result for the same "
                         "parameters already exists under results/")
    ap.add_argument("--format", default="pdf,png",
                    help="comma separated output formats (default pdf,png)")
    ap.add_argument("--no-stamp", action="store_true",
                    help="omit the parameter stamp in the figure corner")
    return ap


def provenance(args=None, detail: str | None = None) -> str:
    """One line stating the parameters the figure was produced with."""
    if detail is None:
        detail = f"{TGS_RUNS} runs/point, {HORIZON_ROUNDS // 86400} d horizon"
    return f"generated on this machine ({detail})"


# --------------------------------------------------------------------------
# Running the data-generation stage
# --------------------------------------------------------------------------
def run(cmd, cwd=None):
    """Run a subprocess, streaming its output, and abort the script on failure."""
    printable = " ".join(str(c) for c in cmd)
    print(f"\n$ {printable}", flush=True)
    proc = subprocess.run([str(c) for c in cmd], cwd=str(cwd) if cwd else None)
    if proc.returncode != 0:
        raise SystemExit(f"command failed ({proc.returncode}): {printable}")


def _cached(out_path: Path, key: dict, force: bool) -> bool:
    """True if ``out_path`` already holds a result computed with ``key``.

    Several targets share a dataset (Figures 6, 7, 8 and Table 3 all read the
    P_norm = 0.999 sweeps), so a re-run of `all` would otherwise recompute the
    same multi-minute sweep four times.
    """
    meta = out_path.with_suffix(out_path.suffix + ".meta.json")
    if force or not out_path.exists() or not meta.exists():
        return False
    try:
        if json.loads(meta.read_text()) != key:
            return False
    except (OSError, ValueError):
        return False
    print(f"[cache] reusing {out_path.relative_to(ROOT)} "
          f"(pass --force to recompute)")
    return True


def _mark(out_path: Path, key: dict):
    meta = out_path.with_suffix(out_path.suffix + ".meta.json")
    meta.write_text(json.dumps(key, indent=2, sort_keys=True))


def tgs_run(out_path: Path, *, attack, alphas, betas, faulty, p_norm,
            p_actual=None, no_flag=False, sims, rounds, jobs, force=False):
    """Invoke the TGS Monte-Carlo simulator into ``out_path``."""
    key = dict(kind="tgs", attack=attack, alphas=list(alphas), betas=list(betas),
               faulty=faulty, p_norm=p_norm, p_actual=p_actual, no_flag=no_flag,
               sims=sims, rounds=rounds)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    if _cached(out_path, key, force):
        return out_path

    cmd = [sys.executable, SRC / "tgs" / "tgs_sim.py",
           "--out", out_path,
           "--attack", attack,
           "--alphas", ",".join(str(a) for a in alphas),
           "--betas", ",".join(str(b) for b in betas),
           "-f", faulty,
           "--p-norm", p_norm,
           "--sims", sims,
           "--rounds", rounds,
           "--jobs", jobs]
    if p_actual is not None:
        cmd += ["--p-actual", p_actual]
    if no_flag:
        cmd += ["--no-flag"]
    run(cmd)
    _mark(out_path, key)
    return out_path


def pistis_build():
    """Build the PISTIS round simulator; returns the path to the binary."""
    d = SRC / "pistis-sim"
    run(["make", "-s"], cwd=d)
    return d / "sim"


def pistis_run(out_path: Path, *, timeouts, instances, rounds, jobs, loss=0.001,
               force=False):
    key = dict(kind="pistis", timeouts=list(timeouts), instances=instances,
               rounds=rounds, loss=loss)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    if _cached(out_path, key, force):
        return out_path

    binary = pistis_build()
    run([binary,
         "--timeouts", ",".join(str(t) for t in timeouts),
         "--instances", instances,
         "--rounds", rounds,
         "--loss", loss,
         "--threads", jobs,
         "--out", out_path])
    _mark(out_path, key)
    return out_path


def ns3_root() -> Path:
    """Where the ns-3 tree Figure 10 builds against is expected to live."""
    return Path(os.environ.get("NS3_ROOT", ROOT.parent / "railway-ns3")).resolve()


def ns3_available() -> bool:
    """True if that tree has the prebuilt headers the scenario compiles against."""
    return (ns3_root() / "build" / "include" / "ns3").is_dir()


def railway_run(out_dir: Path, *, end_time=400, jobs=4, force=False):
    """Rebuild and run the three ns-3 railway scenarios into ``out_dir``."""
    key = dict(kind="railway", end_time=end_time,
               scenarios=["stable", "manual", "heimdallr"])
    marker = out_dir / "scenarios.meta.json"
    if not force and marker.exists():
        try:
            if json.loads(marker.read_text()) == key:
                print(f"[cache] reusing {out_dir.relative_to(ROOT)} "
                      f"(pass --force to recompute)")
                return out_dir
        except (OSError, ValueError):
            pass

    out_dir.mkdir(parents=True, exist_ok=True)
    run([sys.executable, SRC / "railway" / "run_scenarios.py",
         "--out", out_dir, "--end-time", end_time, "--jobs", min(jobs, 8)])
    marker.write_text(json.dumps(key, indent=2, sort_keys=True))
    return out_dir


# --------------------------------------------------------------------------
# Loading
# --------------------------------------------------------------------------
def load_tgs(path: Path) -> pd.DataFrame:
    if not Path(path).exists():
        raise SystemExit(
            f"missing data file: {path}\n"
            f"Run ./run.sh <target> to generate it."
        )
    return pd.read_csv(path, names=TGS_COLUMNS, header=0)


# --------------------------------------------------------------------------
# Plotting
# --------------------------------------------------------------------------
def tgs_heatmap(ax, df: pd.DataFrame, value: str, cmap: str, *, annotate=True):
    """Draw one alpha x beta TGS heat-map panel, matching the paper's style."""
    pivot = df.pivot(index="alpha", columns="beta", values=value)
    data = pivot.values

    im = ax.imshow(data, aspect="auto", cmap=cmap, origin="lower",
                   vmin=0, vmax=1)
    ax.set_xlabel(r"beta ($\beta$)", fontsize=14)
    ax.set_ylabel(r"alpha ($\alpha$)", fontsize=14)
    ax.set_xticks(range(len(pivot.columns)))
    ax.set_xticklabels([f"{c:g}" for c in pivot.columns])
    ax.set_yticks(range(len(pivot.index)))
    ax.set_yticklabels([f"{i:g}" for i in pivot.index])

    if annotate:
        for i in range(data.shape[0]):
            for j in range(data.shape[1]):
                ax.text(j, i, f"{data[i, j]:.3f}", ha="center", va="center",
                        color="white" if data[i, j] > 0.5 else "black",
                        fontsize=9)
    return im


def tgs_figure(panels, *, suptitle=None):
    """Build a 2- or 4-panel TGS figure.

    ``panels`` is a list of dicts with keys: df, value, cmap, title.
    """
    n = len(panels)
    ncols = 2
    nrows = (n + 1) // 2
    fig, axes = plt.subplots(nrows, ncols, figsize=(6.0 * ncols, 3.4 * nrows))
    axes = np.atleast_1d(axes).ravel()

    for ax, panel in zip(axes, panels):
        im = tgs_heatmap(ax, panel["df"], panel["value"], panel["cmap"])
        ax.set_title(panel["title"], fontsize=13)
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.03)
    for ax in axes[n:]:
        ax.axis("off")

    if suptitle:
        fig.suptitle(suptitle, fontsize=14)
    fig.tight_layout()
    return fig


def save(fig, name: str, args, detail: str | None = None) -> list[Path]:
    """Save ``fig`` into outputs/ in every requested format."""
    OUTPUTS.mkdir(parents=True, exist_ok=True)
    if not getattr(args, "no_stamp", False):
        fig.text(0.995, -0.06, provenance(args, detail), ha="right", va="top",
                 fontsize=7, color="0.45")
    written = []
    for ext in [e.strip() for e in args.format.split(",") if e.strip()]:
        path = OUTPUTS / f"{name}.{ext}"
        fig.savefig(path, dpi=200)
        written.append(path)
    plt.close(fig)
    print("\nwrote:")
    for p in written:
        print(f"  {p.relative_to(ROOT)}")
    return written


def save_table(text: str, name: str) -> Path:
    OUTPUTS.mkdir(parents=True, exist_ok=True)
    path = OUTPUTS / f"{name}.md"
    path.write_text(text)
    print(text)
    print(f"\nwrote:\n  {path.relative_to(ROOT)}")
    return path


def banner(args, target: str):
    print("=" * 72)
    print(f"{target}   {TGS_RUNS} runs/point, {HORIZON_ROUNDS // 86400} d horizon, "
          f"jobs={args.jobs}")
    print("=" * 72)
