#!/usr/bin/env python3
"""Table 4 - total CPU time per network measurement round: MS-PTP vs HEIMDALLR.

Two regions run one measurement round; the table sums the CPU time spent by
every node on message transmission, signature generation/verification,
timestamp processing and latency estimation.  MS-PTP needs 3f+1 nodes per
region against HEIMDALLR's f+1, which is where most of the gap comes from.
"""
import ast
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common"))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "src"))

import bench  # noqa: E402
import repro  # noqa: E402

F_VALUES = [1, 2, 3, 4]
ROWS = [("MS-PTP", "ms-ptp", "network/ms-ptp.txt"),
        ("HEIMDALLR", "heimdallr", "network/heimdallr.txt")]


def reference_values(rel: str):
    """The recorded results.txt content is a Python list of microsecond totals."""
    return ast.literal_eval((repro.REFERENCE / rel).read_text().strip())


def main():
    args = repro.make_parser(__doc__).parse_args()
    repro.banner(args, "Table 4")

    log_root = repro.RESULTS / "network-logs"
    print(f"Running the localhost measurement benchmarks into {log_root}\n"
          f"({repro.SETTLE_S}s recorded per configuration).\n")
    bench.main(["--log-root", str(log_root),
                "--protocols", ",".join(bench.TABLE4_ORDER),
                "--f-values", ",".join(str(f) for f in F_VALUES),
                "--settle", str(repro.SETTLE_S)])
    values = {key: [bench.overhead_us(log_root, bench.PROTOCOLS[key], f)
                    for f in F_VALUES]
              for _, key, _ in ROWS}

    head = "| f_i for both regions | " + " | ".join(str(f) for f in F_VALUES) + " |"
    sep = "|---" * (len(F_VALUES) + 1) + "|"
    lines = [head, sep]
    for label, key, _ in ROWS:
        lines.append(f"| **{label} (ms)** | " +
                     " | ".join(f"{v / 1000:.2f}" for v in values[key]) + " |")

    ratios = " | ".join(
        f"{values['ms-ptp'][i] / values['heimdallr'][i]:.2f}x"
        for i in range(len(F_VALUES)))

    print(f"\nMS-PTP / HEIMDALLR: {ratios}")
    print("Published: MS-PTP 11.30 / 30.93 / 61.28 / 102.05 ms;")
    print("           HEIMDALLR 2.36 / 6.34 / 12.93 / 22.67 ms (up to 4.88x lower).")

    text = "\n".join([
        "# Table 4: Total CPU time per network measurement round",
        "",
        *lines,
        "",
    ])
    repro.save_table(text, "table-04")


if __name__ == "__main__":
    main()
