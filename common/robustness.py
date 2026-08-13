"""Shared data loading for Table 3 and Figure 8 (HEIMDALLR vs PISTIS).

Both report the same two quantities - Pr(T_robust >= 30 days) and the
normalised mean T_robust - for HEIMDALLR at T = d and for PISTIS at several
timeout-to-heartbeat ratios T/d.  Figure 8 then extrapolates them to N
independent region pairs.
"""

from __future__ import annotations

import pandas as pd

from repro import HORIZON_ROUNDS, PISTIS_RUNS, RESULTS, pistis_run
import tgs_target as T

# The operating point the paper evaluates HEIMDALLR at (Sec. VI-A).
HEIMDALLR_ALPHA = 0.1
HEIMDALLR_BETA = 5
PISTIS_TIMEOUTS = [8, 12, 14, 16]


def pistis(args) -> pd.DataFrame:
    """Pr / mean T_robust for PISTIS, indexed by timeout ratio T/d."""
    path = pistis_run(RESULTS / "pistis" / "robustness.csv",
                      timeouts=PISTIS_TIMEOUTS, instances=PISTIS_RUNS,
                      rounds=HORIZON_ROUNDS, jobs=args.jobs, force=args.force)
    return pd.read_csv(path).set_index("timeout_rounds")


def heimdallr(args) -> dict:
    """Pr / mean T_robust for HEIMDALLR at (alpha, beta) = (0.1, 5).

    Reported per adversary plus the worst case over both, which is the number
    quoted in Table III.
    """
    out = {}
    for name, spec in (("adaptive", T.FIG67_ADAPTIVE),
                       ("aggressive", T.FIG67_AGGRESSIVE)):
        df = T.dataset(spec, args)
        row = df[(df["alpha"] == HEIMDALLR_ALPHA) & (df["beta"] == HEIMDALLR_BETA)]
        if row.empty:
            raise SystemExit(
                f"(alpha, beta) = ({HEIMDALLR_ALPHA}, {HEIMDALLR_BETA}) is not in the "
                f"{name} sweep {spec.path()}")
        out[name] = (float(row["survive_rate"].iloc[0]),
                     float(row["avg_survive_time"].iloc[0]))
    out["worst"] = (min(v[0] for v in out.values()),
                    min(v[1] for v in out.values()))
    return out
