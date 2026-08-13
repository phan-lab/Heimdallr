#!/usr/bin/env python3
"""Figure 7 - normalized mean T_robust with TGS (P_norm = 0.999, f_i = 1)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import repro  # noqa: E402
import tgs_target as T  # noqa: E402


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 7")

    adaptive = T.dataset(T.FIG67_ADAPTIVE, args)
    aggressive = T.dataset(T.FIG67_AGGRESSIVE, args)
    _, no_tgs_mean = T.baseline(T.FIG67_BASELINE, args)

    fig = repro.tgs_figure([
        dict(df=adaptive, value="avg_survive_time", cmap="Blues",
             title="(a) Adaptive attack"),
        dict(df=aggressive, value="avg_survive_time", cmap="Blues",
             title="(b) Aggressive attack"),
    ], suptitle=r"Fig. 7: Normalized mean time $T_{robust}$ with TGS "
                rf"($f_i = 1$, $P_{{norm}} = 0.999$).  Without TGS: {no_tgs_mean:.3f}.")

    repro.save(fig, "figure-07", args)


if __name__ == "__main__":
    main()
