#!/usr/bin/env python3
"""Figure 14 - effectiveness of TGS under a network DoS attack (f_i = 1).

TGS is configured for P_norm = 0.999 (the value measured on the testbed), but
at run time the DoS degrades the network to P_norm = 0.99 - a 10x increase in
jitter - so the reward/penalty ratio is now mis-calibrated for the traffic the
nodes actually see.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import repro  # noqa: E402
import tgs_target as T  # noqa: E402


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 14")

    adaptive = T.dataset(T.FIG14_ADAPTIVE, args)
    aggressive = T.dataset(T.FIG14_AGGRESSIVE, args)
    pr, mean = T.baseline(T.FIG14_BASELINE, args)

    fig = repro.tgs_figure([
        dict(df=adaptive, value="survive_rate", cmap="Greens",
             title=f"(a) Probability, adaptive attack (without TGS: {pr:.3f})"),
        dict(df=aggressive, value="survive_rate", cmap="Greens",
             title=f"(b) Probability, aggressive attack (without TGS: {pr:.3f})"),
        dict(df=adaptive, value="avg_survive_time", cmap="Blues",
             title=f"(c) Normalized mean time, adaptive (without TGS: {mean:.3f})"),
        dict(df=aggressive, value="avg_survive_time", cmap="Blues",
             title=f"(d) Normalized mean time, aggressive (without TGS: {mean:.3f})"),
    ], suptitle=r"Fig. 14: Effectiveness of TGS under network DoS ($f_i = 1$, "
                r"configured $P_{norm} = 0.999$, delivered $P_{norm} = 0.99$)")

    repro.save(fig, "figure-14", args)


if __name__ == "__main__":
    main()
