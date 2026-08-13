"""Declarative specs for the five TGS heat-map figures (6, 7, 12, 13, 14).

Each figure is a pair of (alpha x beta) parameter sweeps - one per adversary -
that differ only in P_norm, f_i and the grid.  In the original code base those
knobs lived in ``main()`` of ``adaptive-attack.py`` / ``aggressive-attack.py``
and were edited by hand between runs, so the sweeps below are the single place
where "which figure used which parameters" is now written down.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from repro import (HORIZON_ROUNDS, REFERENCE, RESULTS, TGS_RUNS, load_tgs,
                   tgs_run)

ALPHAS_6 = [0.001, 0.01, 0.1, 0.2, 0.5, 1.0]
ALPHAS_5 = [0.001, 0.01, 0.1, 0.5, 1.0]
BETAS_ADAPTIVE = [3, 5, 10, 20, 50]
BETAS_AGGRESSIVE = [1, 2, 3, 5, 10, 20, 50]


@dataclass
class TgsSpec:
    """One alpha x beta sweep."""
    key: str                 # output basename under results/tgs/
    attack: str              # 'adaptive' | 'aggressive'
    alphas: list
    betas: list
    faulty: int              # f_i
    p_norm: float            # P_norm TGS was configured with
    reference: str           # recorded data under data/reference/tgs/
    p_actual: float | None = None   # actual P_norm (DoS); None -> == p_norm
    label: str = ""

    def path(self) -> Path:
        """Where this sweep's generated data lives."""
        return RESULTS / "tgs" / f"{self.key}.csv"

    def reference_path(self) -> Path:
        """The authors' recording of the same sweep, for ./compare.py only."""
        return REFERENCE / "tgs" / self.reference


@dataclass
class Baseline:
    """The 'without TGS' scalar quoted in each figure caption."""
    faulty: int
    p_norm: float
    p_actual: float | None = None
    key: str = "baseline"
    # The paper's published values, kept for reference in the docs only;
    # the baseline is recomputed on every run.
    survive_rate: float = float("nan")
    avg_survive_time: float = float("nan")


# --------------------------------------------------------------------------
# Figure 6 & 7 - P_norm = 0.999, f_i = 1
# --------------------------------------------------------------------------
FIG67_ADAPTIVE = TgsSpec(
    key="p999-f1-adaptive", attack="adaptive", alphas=ALPHAS_6,
    betas=BETAS_ADAPTIVE, faulty=1, p_norm=0.999,
    reference="result-flag-999-adaptive.txt", label="adaptive attack")
FIG67_AGGRESSIVE = TgsSpec(
    key="p999-f1-aggressive", attack="aggressive", alphas=ALPHAS_6,
    betas=BETAS_AGGRESSIVE, faulty=1, p_norm=0.999,
    reference="result-flag-999-aggressive.txt", label="aggressive attack")
FIG67_BASELINE = Baseline(faulty=1, p_norm=0.999, key="p999-f1",
                          survive_rate=0.143, avg_survive_time=0.268)

# --------------------------------------------------------------------------
# Figure 12 - P_norm = 0.99, f_i = 1
# --------------------------------------------------------------------------
FIG12_ADAPTIVE = TgsSpec(
    key="p99-f1-adaptive", attack="adaptive", alphas=ALPHAS_6,
    betas=BETAS_ADAPTIVE, faulty=1, p_norm=0.99,
    reference="result-flag-99-adaptive.txt", label="adaptive attack")
FIG12_AGGRESSIVE = TgsSpec(
    key="p99-f1-aggressive", attack="aggressive", alphas=ALPHAS_6,
    betas=BETAS_AGGRESSIVE, faulty=1, p_norm=0.99,
    reference="result-flag-99-aggressive.txt", label="aggressive attack")
FIG12_BASELINE = Baseline(faulty=1, p_norm=0.99, key="p99-f1",
                          survive_rate=0.108, avg_survive_time=0.112)

# --------------------------------------------------------------------------
# Figure 13 - P_norm = 0.99, f_i = 2
# --------------------------------------------------------------------------
FIG13_ADAPTIVE = TgsSpec(
    key="p99-f2-adaptive", attack="adaptive", alphas=ALPHAS_5,
    betas=[5, 10, 20, 30, 50], faulty=2, p_norm=0.99,
    reference="result-flag-f2-ada.txt", label="adaptive attack")
FIG13_AGGRESSIVE = TgsSpec(
    key="p99-f2-aggressive", attack="aggressive", alphas=ALPHAS_6,
    betas=[1, 2, 5, 10, 20, 30, 50], faulty=2, p_norm=0.99,
    reference="result-flag-f2-agg.txt", label="aggressive attack")
FIG13_BASELINE = Baseline(faulty=2, p_norm=0.99, key="p99-f2",
                          survive_rate=0.486, avg_survive_time=0.505)

# --------------------------------------------------------------------------
# Figure 14 - network DoS: configured for P_norm = 0.999, delivered 0.99
# --------------------------------------------------------------------------
FIG14_ADAPTIVE = TgsSpec(
    key="dos-f1-adaptive", attack="adaptive", alphas=ALPHAS_5,
    betas=BETAS_ADAPTIVE, faulty=1, p_norm=0.999, p_actual=0.99,
    reference="result-flag-ada-dos.txt", label="adaptive attack")
FIG14_AGGRESSIVE = TgsSpec(
    key="dos-f1-aggressive", attack="aggressive", alphas=ALPHAS_6,
    betas=BETAS_AGGRESSIVE, faulty=1, p_norm=0.999, p_actual=0.99,
    reference="result-flag-agg-dos.txt", label="aggressive attack")
FIG14_BASELINE = Baseline(faulty=1, p_norm=0.999, p_actual=0.99, key="dos-f1",
                          survive_rate=0.108, avg_survive_time=0.112)


# --------------------------------------------------------------------------
def dataset(spec: TgsSpec, args):
    """Generate ``spec``'s sweep at the paper's parameters and return it."""
    path = spec.path()
    tgs_run(path, attack=spec.attack, alphas=spec.alphas, betas=spec.betas,
            faulty=spec.faulty, p_norm=spec.p_norm, p_actual=spec.p_actual,
            sims=TGS_RUNS, rounds=HORIZON_ROUNDS, jobs=args.jobs,
            force=args.force)
    return load_tgs(path)


def baseline(bl: Baseline, args):
    """Return (survive_rate, avg_survive_time) for the 'without TGS' case.

    With flagging disabled the aggressive adversary attacks every message it is
    assigned regardless of its score, so the result does not depend on
    (alpha, beta); one grid point is enough.
    """
    path = RESULTS / "tgs" / f"no-tgs-{bl.key}.csv"
    tgs_run(path, attack="aggressive", alphas=[0.1], betas=[5],
            faulty=bl.faulty, p_norm=bl.p_norm, p_actual=bl.p_actual,
            no_flag=True, sims=TGS_RUNS, rounds=HORIZON_ROUNDS,
            jobs=args.jobs, force=args.force)
    row = load_tgs(path).iloc[0]
    return float(row["survive_rate"]), float(row["avg_survive_time"])
