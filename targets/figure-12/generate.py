#!/usr/bin/env python3
"""Figure 12 - effectiveness of TGS with P_norm = 0.99 and f_i = 1."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import repro  # noqa: E402
import tgs_target as T  # noqa: E402


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 12")

    adaptive = T.dataset(T.FIG12_ADAPTIVE, args)
    aggressive = T.dataset(T.FIG12_AGGRESSIVE, args)
    pr, mean = T.baseline(T.FIG12_BASELINE, args)

    fig = repro.tgs_figure([
        dict(df=adaptive, value="survive_rate", cmap="Greens",
             title=f"(a) Probability, adaptive attack (without TGS: {pr:.3f})"),
        dict(df=aggressive, value="survive_rate", cmap="Greens",
             title=f"(b) Probability, aggressive attack (without TGS: {pr:.3f})"),
        dict(df=adaptive, value="avg_survive_time", cmap="Blues",
             title=f"(c) Normalized mean time, adaptive (without TGS: {mean:.3f})"),
        dict(df=aggressive, value="avg_survive_time", cmap="Blues",
             title=f"(d) Normalized mean time, aggressive (without TGS: {mean:.3f})"),
    ], suptitle=r"Fig. 12: Effectiveness of TGS with $P_{norm} = 0.99$ and $f_i = 1$")

    repro.save(fig, "figure-12", args)


if __name__ == "__main__":
    main()
