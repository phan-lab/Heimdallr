#!/usr/bin/env python3
"""Figure 6 - Pr(T_robust >= 30 days) with TGS (P_norm = 0.999, f_i = 1)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import repro  # noqa: E402
import tgs_target as T  # noqa: E402


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Figure 6")

    adaptive = T.dataset(T.FIG67_ADAPTIVE, args)
    aggressive = T.dataset(T.FIG67_AGGRESSIVE, args)
    no_tgs_pr, _ = T.baseline(T.FIG67_BASELINE, args)

    fig = repro.tgs_figure([
        dict(df=adaptive, value="survive_rate", cmap="Greens",
             title="(a) Adaptive attack"),
        dict(df=aggressive, value="survive_rate", cmap="Greens",
             title="(b) Aggressive attack"),
    ], suptitle=r"Fig. 6: Pr($T_{robust} \geq$ 30 days) with TGS ($f_i = 1$, "
                rf"$P_{{norm}} = 0.999$).  Probability without TGS is {no_tgs_pr:.3f}.")

    repro.save(fig, "figure-06", args)


if __name__ == "__main__":
    main()
