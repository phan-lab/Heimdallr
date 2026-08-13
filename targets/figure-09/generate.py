#!/usr/bin/env python3
"""Figure 9 - total CPU time per inter-region job, HEIMDALLR vs BFT protocols.

Each protocol is run on 127.0.0.1 as n server (and, where the protocol has a
downstream region, n client) processes.  Every process records the CPU time it
spends on protocol work per job; the figure sums those means over all processes
and adds the application execution time the replication factor forces the
system to repeat.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "src"))

import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

import bench  # noqa: E402
import repro  # noqa: E402

F_VALUES = [1, 2, 3, 4]          # plotted as F = 2f faulty nodes system-wide
MARKERS = ["x", ".", "*", "o", "s", "D"]
PANELS = [(1, "Task Exec. Time = 200 us"), (3, "Task Exec. Time = 1 ms")]


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 9")

    log_root = repro.RESULTS / "protocol-logs"
    configs = len(bench.FIGURE9_ORDER) * len(F_VALUES)
    print(f"Running the localhost benchmarks into {log_root}\n"
          f"({repro.SETTLE_S}s recorded per configuration, "
          f"{repro.SETTLE_S_PISTIS}s for PISTIS, {configs} configurations).\n")
    bench.main(["--log-root", str(log_root),
                "--f-values", ",".join(str(f) for f in F_VALUES)])

    results = bench.analyse(log_root, f_values=F_VALUES)
    if not results:
        raise SystemExit(f"no benchmark data under {log_root}")

    # Dump the analysed numbers next to the figure so they can be read without
    # re-deriving them from the raw .stats logs.
    repro.OUTPUTS.mkdir(parents=True, exist_ok=True)
    (repro.OUTPUTS / "figure-09-data.json").write_text(json.dumps(results, indent=2))

    fig, axes = plt.subplots(1, 2, figsize=(7.0, 3.6))
    fig.subplots_adjust(wspace=0.32, bottom=0.30, top=0.90)

    for ax, (idx, title) in zip(axes, PANELS):
        for i, name in enumerate(bench.FIGURE9_ORDER):
            if name not in results:
                continue
            r = results[name]
            fs = [f for f in r["f"] if f in F_VALUES]
            ys = [r["total_us"][r["f"].index(f)][idx] / 1000.0 for f in fs]
            ax.plot([2 * f for f in fs], ys, marker=MARKERS[i], label=r["label"])
        ax.set_title(title, fontsize=13)
        ax.set_yticks(np.arange(0, 401, 100))
        ax.set_ylim(-5, 400)
        ax.set_xticks([2 * f for f in F_VALUES])
        ax.grid(linestyle="--")

    axes[0].set_ylabel("Total CPU Time (ms)", fontsize=14)
    fig.supxlabel("Maximum Number of Faulty Nodes in the System", fontsize=14, y=0.16)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, fontsize=11, loc="upper center",
               bbox_to_anchor=(0.5, 0.13), ncol=3, frameon=True,
               handletextpad=0.3, columnspacing=1)

    for name in bench.FIGURE9_ORDER:
        if name in results:
            r = results[name]
            vals = ", ".join(f"F={2*f}: {r['total_us'][r['f'].index(f)][1]/1000:.2f}"
                             for f in r["f"] if f in F_VALUES)
            print(f"  {r['label']:28s} (200 us) {vals}")

    repro.save(fig, "figure-09", args,
               detail=f"{repro.SETTLE_S}s recorded per configuration "
                      f"({repro.SETTLE_S_PISTIS}s for PISTIS)")


if __name__ == "__main__":
    main()
