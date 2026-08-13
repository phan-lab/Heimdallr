#!/usr/bin/env python3
"""Figure 8 - Pr(T_robust >= 30 days) with a varying number of region pairs.

Region pairs fail independently, so the probability that *no* pair leaves
normal operation over the horizon is the single-pair probability raised to the
number of pairs.  The single-pair probabilities are the ones in Table 3.

The figure itself is a calculation, as it is in the paper - p^n, with no
simulation of its own.  Its inputs are the Figure 6 sweeps and the Table 3
PISTIS runs, which it generates (or reuses from cache) like any other target.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import math  # noqa: E402

import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

import repro  # noqa: E402
import robustness  # noqa: E402

# Exactly the sweep the paper plots (micros/reputation/Untitled-1.ipynb cell 9).
# Do not add n = 1: at n = 1 the PISTIS T = 12d series is its raw single-pair
# probability, 0.534, which makes that curve start high and look nothing like
# the published figure - there the leftmost point is 0.534^5 = 0.043.
NUM_REGION_PAIRS = [5, 10, 25, 50, 100, 150, 200]

def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 8")

    # This figure extrapolates rather than measures, but its inputs are real:
    # the sweeps below are generated (or reused from cache) exactly as Figure 6
    # and Table 3 generate them.
    h = robustness.heimdallr(args)
    p = robustness.pistis(args)

    n = np.array(NUM_REGION_PAIRS, dtype=float)
    series = [
        (r"Heimdallr ($\mathbb{T}=d$)", h["worst"][0], "x", "-"),
        (r"PISTIS ($\mathbb{T}=16d$)", float(p.loc[16, "survive_rate"]), "^", ":"),
        (r"PISTIS ($\mathbb{T}=14d$)", float(p.loc[14, "survive_rate"]), "s", "-."),
        (r"PISTIS ($\mathbb{T}=12d$)", float(p.loc[12, "survive_rate"]), "o", "--"),
    ]

    # Raising a probability to the 200th power multiplies its relative
    # uncertainty by ~200, so report what the exponentiation does to the
    # sampling error even at the paper's 10,000 runs.
    worst = max(
        (max(NUM_REGION_PAIRS) * math.sqrt(max(b * (1 - b), 1e-12) / repro.TGS_RUNS) / b,
         label) for label, b, _, _ in series)
    print(f"  (exponentiating amplifies sampling error ~{max(NUM_REGION_PAIRS)}x; "
          f"worst case here is {worst[0] * 100:.0f}% on {worst[1]})")

    fig, ax = plt.subplots(figsize=(5.4, 3.0))
    for label, base, marker, ls in series:
        ax.plot(n, base ** n, marker=marker, linestyle=ls, label=label)
        print(f"  {label:32s} single-pair Pr = {base:.4f}  ->  "
              f"Pr at 200 pairs = {base ** 200:.4f}")

    ax.set_xlabel("Number of Region Pairs", fontsize=14)
    ax.set_ylabel("Probability", fontsize=14)
    ax.set_ylim(0, 1.05)
    ax.grid(linestyle="--")
    ax.legend(fontsize=11, loc="center right", bbox_to_anchor=(1, 0.45))
    fig.tight_layout()

    repro.save(fig, "figure-08", args)


if __name__ == "__main__":
    main()
