#!/usr/bin/env python3
"""Table 3 - HEIMDALLR vs PISTIS operational robustness."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))

import repro  # noqa: E402
import robustness  # noqa: E402


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Table 3")

    h = robustness.heimdallr(args)
    p = robustness.pistis(args)

    cols = [("HEIMDALLR (T = d)", h["worst"])]
    cols += [(f"PISTIS (T = {t}d)",
              (float(p.loc[t, "survive_rate"]), float(p.loc[t, "avg_survive_time"])))
             for t in robustness.PISTIS_TIMEOUTS]

    header = "| System | " + " | ".join(name for name, _ in cols) + " |"
    sep = "|---" * (len(cols) + 1) + "|"
    row_pr = "| **Pr(T_robust >= 30 days)** | " + \
             " | ".join(f"{v[0]:.3f}" for _, v in cols) + " |"
    row_mean = "| **Normalized mean T_robust** | " + \
               " | ".join(f"{v[1]:.3f}" for _, v in cols) + " |"

    # The file holds the table and nothing else.  Everything else - how
    # HEIMDALLR's number was picked, how many runs are behind each row, the
    # published values - goes to the console, where it informs the person
    # running it without ending up in the deliverable.
    print(f"\nHEIMDALLR at (alpha, beta) = "
          f"({robustness.HEIMDALLR_ALPHA}, {robustness.HEIMDALLR_BETA}), "
          f"worst case over the two adversaries:")
    print(f"  adaptive:   Pr = {h['adaptive'][0]:.4f}, mean = {h['adaptive'][1]:.4f}")
    print(f"  aggressive: Pr = {h['aggressive'][0]:.4f}, mean = {h['aggressive'][1]:.4f}")
    print(f"PISTIS: {int(p['instances'].iloc[0])} runs per T/d over "
          f"{int(p['rounds'].iloc[0]) // 86400} simulated days.")
    print("Published: HEIMDALLR 0.999 / 0.999; PISTIS 0.000 / 0.003 (8d), "
          "0.534 / 0.742 (12d),\n           0.978 / 0.988 (14d), 0.999 / 0.999 (16d).")

    text = "\n".join([
        "# Table 3: HEIMDALLR vs PISTIS operational robustness",
        "",
        header, sep, row_pr, row_mean,
        "",
    ])
    repro.save_table(text, "table-03")


if __name__ == "__main__":
    main()
