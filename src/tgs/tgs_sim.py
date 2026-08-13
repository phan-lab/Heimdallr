#!/usr/bin/env python3
"""Timeliness Governing System (TGS) Monte-Carlo simulator.

Unified, fully parameterised replacement for the original
``micros/reputation/{adaptive-attack,aggressive-attack}.py`` scripts, whose
parameters (P_norm, f, n, the alpha/beta grid, the attack strategy and whether
flagging is enabled) were hard-coded in ``main()`` and edited by hand between
runs.

Everything is now a command-line flag, so one binary reproduces every TGS
figure in the paper (Fig. 6, 7, 12, 13, 14 and the HEIMDALLR row of Table 3).

Output is the same CSV schema the original scripts produced, so the original
plotting code and the recorded reference data stay compatible:

    alpha,beta,avg_survive_time,survive_rate,flag_tp,flag_fp

Model (unchanged from the original):
  * One upstream and one downstream region, each with n = 2f+1 nodes, of which
    f+1 are "primaries" (replicas) currently assigned to the inter-region task.
  * f nodes per region are Byzantine, drawn uniformly at random.
  * Every round each of the (f+1)x(f+1) primary pairs exchanges one message.
    A message is "suspicious" if the network misbehaves (prob. 1 - p_actual) or
    if a Byzantine endpoint chooses to attack.
  * Suspicious -> both endpoints lose s_pen; normal -> both gain s_awd, capped
    at s_max.
  * A node whose score reaches <= 0 is flagged, its flag counter increases and
    it is replaced by the non-primary with the lowest flag counter (TGS).
  * The system leaves normal operation the first round in which *every* pair
    was suspicious (no correct output delivered at all -> safe-mode).

Attack strategies:
  aggressive  a Byzantine node attacks on every message it is assigned to.
  adaptive    a Byzantine node backs off before it would be flagged: it only
              attacks while its score is still above 2*s_pen.  When
              (f+1)*s_pen >= s_init a single round suffices to drain a score,
              so backing off gains nothing and the adaptive attacker behaves
              aggressively.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
import time
from multiprocessing import Pool

import numpy as np

try:
    from numba import njit
except ImportError:  # pragma: no cover - numba is listed in requirements.txt
    print("numba not found; falling back to pure Python (very slow)", file=sys.stderr)

    def njit(*a, **kw):
        def deco(fn):
            return fn

        return deco if not a else a[0]


ATTACK_AGGRESSIVE = 0
ATTACK_ADAPTIVE = 1

ROUNDS_30_DAYS = 86400 * 30  # 1 s heartbeat period -> 1 round/s for 30 days


@njit(cache=True)
def simulate_core(rounds, n, f, p_actual, s_init, s_max, s_pen, s_awd, seed,
                  use_flag, attack_mode):
    """One simulation run.  Returns (rounds_survived, flag_tp, flag_fp)."""
    np.random.seed(seed)

    num_primaries = f + 1

    upstream_scores = np.full(n, s_init, dtype=np.float64)
    downstream_scores = np.full(n, s_init, dtype=np.float64)

    upstream_ctr = np.zeros(n, dtype=np.int64)
    downstream_ctr = np.zeros(n, dtype=np.int64)

    upstream_primary = np.arange(num_primaries).astype(np.int64)
    downstream_primary = np.arange(num_primaries).astype(np.int64)

    upstream_faulty = np.zeros(n, dtype=np.bool_)
    downstream_faulty = np.zeros(n, dtype=np.bool_)
    upstream_faulty[np.random.choice(n, f, replace=False)] = True
    downstream_faulty[np.random.choice(n, f, replace=False)] = True

    # An adaptive attacker cannot dodge flagging once one round of penalties is
    # enough to drain a full score; it then behaves like the aggressive one.
    adaptive_can_dodge = attack_mode == ATTACK_ADAPTIVE and num_primaries * s_pen < s_init

    n_flag_tp = 0
    n_flag_fp = 0

    for round_i in range(rounds):
        round_all_suspicious = True

        for up_idx in range(num_primaries):
            up = upstream_primary[up_idx]
            for down_idx in range(num_primaries):
                down = downstream_primary[down_idx]

                if np.random.random() > p_actual:
                    suspicious = True          # network misbehaviour
                elif adaptive_can_dodge:
                    suspicious = ((upstream_faulty[up] and upstream_scores[up] > 2 * s_pen)
                                  or (downstream_faulty[down] and downstream_scores[down] > 2 * s_pen))
                else:
                    suspicious = upstream_faulty[up] or downstream_faulty[down]

                if suspicious:
                    upstream_scores[up] -= s_pen
                    downstream_scores[down] -= s_pen
                else:
                    upstream_scores[up] = min(upstream_scores[up] + s_awd, s_max)
                    downstream_scores[down] = min(downstream_scores[down] + s_awd, s_max)
                    round_all_suspicious = False

        if round_all_suspicious:
            # No inter-region message got through this round -> safe mode.
            return round_i + 1, n_flag_tp, n_flag_fp

        if not use_flag:
            continue

        for side in range(2):
            if side == 0:
                scores, ctr, primary, faulty = (upstream_scores, upstream_ctr,
                                                upstream_primary, upstream_faulty)
            else:
                scores, ctr, primary, faulty = (downstream_scores, downstream_ctr,
                                                downstream_primary, downstream_faulty)

            for slot in range(num_primaries):
                node = primary[slot]
                if scores[node] > 0:
                    continue

                ctr[node] += 1
                scores[node] = s_init
                if faulty[node]:
                    n_flag_tp += 1
                else:
                    n_flag_fp += 1

                # Replace with the non-primary that has been flagged least
                # often; ties broken by node id (deterministic, as in Alg. 2).
                best = -1
                best_ctr = 0
                for cand in range(n):
                    is_primary = False
                    for j in range(num_primaries):
                        if primary[j] == cand:
                            is_primary = True
                            break
                    if is_primary:
                        continue
                    if best == -1 or ctr[cand] < best_ctr:
                        best = cand
                        best_ctr = ctr[cand]
                if best != -1:
                    primary[slot] = best
                    scores[best] = s_init

    return rounds, n_flag_tp, n_flag_fp


class Simulator:
    """One (alpha, beta) configuration."""

    def __init__(self, alpha, beta, f, n, p_conf, p_actual, use_flag, attack_mode,
                 s_init=1.0, s_max=1.0):
        self.alpha = alpha
        self.beta = beta
        self.f = f
        self.n = n
        self.p_conf = p_conf        # P_norm the operator configured TGS with
        self.p_actual = p_actual    # P_norm the network actually delivers
        self.use_flag = use_flag
        self.attack_mode = attack_mode

        self.s_init = s_init
        self.s_max = s_max
        # s_pen = s_max / beta ;  s_pen / s_awd = alpha * P_norm / (1 - P_norm)
        self.s_pen = s_init / beta
        self.s_awd = self.s_pen / p_conf * (1 - p_conf) / alpha

    def _run_one(self, args):
        rounds, seed = args
        return simulate_core(rounds, self.n, self.f, self.p_actual, self.s_init,
                             self.s_max, self.s_pen, self.s_awd, seed,
                             self.use_flag, self.attack_mode)

    def run(self, num_sims, rounds, jobs, base_seed):
        work = [(rounds, base_seed + i) for i in range(num_sims)]
        if jobs == 1:
            results = [self._run_one(w) for w in work]
        else:
            with Pool(jobs) as pool:
                results = pool.map(self._run_one, work, chunksize=max(1, num_sims // (jobs * 4)))

        results = np.asarray(results, dtype=np.float64)
        survived = results[:, 0]
        return {
            "avg_survive_time": float(np.mean(survived) / rounds),
            "survive_rate": float(np.count_nonzero(survived == rounds) / num_sims),
            "flag_tp": float(np.mean(results[:, 1])),
            "flag_fp": float(np.mean(results[:, 2])),
        }


def _floats(text):
    return [float(x) for x in text.replace(" ", "").split(",") if x]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="output CSV path")
    ap.add_argument("--attack", choices=["adaptive", "aggressive"], default="adaptive")
    ap.add_argument("--alphas", default="0.001,0.01,0.1,0.2,0.5,1.0")
    ap.add_argument("--betas", default="3,5,10,20,50")
    ap.add_argument("-f", "--faulty", type=int, default=1,
                    help="f_i, Byzantine nodes per region (n = 2f+1)")
    ap.add_argument("-n", "--nodes", type=int, default=None,
                    help="nodes per region (default 2f+1)")
    ap.add_argument("--p-norm", type=float, default=0.999,
                    help="P_norm TGS is configured with")
    ap.add_argument("--p-actual", type=float, default=None,
                    help="P_norm the network actually delivers "
                         "(default: same as --p-norm; set lower to model a DoS attack)")
    ap.add_argument("--no-flag", action="store_true",
                    help="disable TGS flagging/reassignment (the 'without TGS' baseline)")
    ap.add_argument("--sims", type=int, default=10000, help="simulations per grid point")
    ap.add_argument("--rounds", type=int, default=ROUNDS_30_DAYS,
                    help="rounds per simulation (default 86400*30 = 30 days at 1 Hz)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--seed", type=int, default=20260811)
    args = ap.parse_args(argv)

    alphas = _floats(args.alphas)
    betas = _floats(args.betas)
    n = args.nodes if args.nodes is not None else 2 * args.faulty + 1
    p_actual = args.p_actual if args.p_actual is not None else args.p_norm
    attack_mode = ATTACK_ADAPTIVE if args.attack == "adaptive" else ATTACK_AGGRESSIVE

    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)

    print(f"[tgs] attack={args.attack} f={args.faulty} n={n} "
          f"P_norm(conf)={args.p_norm} P_norm(actual)={p_actual} "
          f"TGS={'off' if args.no_flag else 'on'}")
    print(f"[tgs] {len(alphas)}x{len(betas)} grid, {args.sims} sims x {args.rounds} rounds, "
          f"{args.jobs} workers -> {args.out}")

    # Warm the JIT once so the per-configuration timings are meaningful.
    t0 = time.time()
    simulate_core(100, 3, 1, 0.999, 1.0, 1.0, 0.1, 0.01, 1, True, ATTACK_ADAPTIVE)
    print(f"[tgs] JIT warm-up {time.time() - t0:.1f}s")

    total = len(alphas) * len(betas)
    done = 0
    started = time.time()
    with open(args.out, "w", newline="") as fh:
        writer = csv.writer(fh)
        writer.writerow(["alpha", "beta", "avg_survive_time", "survive_rate",
                         "flag_tp", "flag_fp"])
        for alpha in alphas:
            for beta in betas:
                done += 1
                sim = Simulator(alpha, beta, args.faulty, n, args.p_norm, p_actual,
                                not args.no_flag, attack_mode)
                t = time.time()
                r = sim.run(args.sims, args.rounds, args.jobs,
                            args.seed + done * 1_000_003)
                writer.writerow([alpha, int(beta) if beta == int(beta) else beta,
                                 f"{r['avg_survive_time']:.6f}",
                                 f"{r['survive_rate']:.6f}",
                                 f"{r['flag_tp']:.2f}", f"{r['flag_fp']:.2f}"])
                fh.flush()
                print(f"[tgs] {done}/{total} alpha={alpha} beta={beta:g} "
                      f"Pr={r['survive_rate']:.4f} mean={r['avg_survive_time']:.4f} "
                      f"({time.time() - t:.1f}s)")

    print(f"[tgs] wrote {args.out} in {time.time() - started:.1f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
