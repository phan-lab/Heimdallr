#!/usr/bin/env bash
# HEIMDALLR artifact reproduction driver.
#
#   ./run.sh                      list the targets
#   ./run.sh all                  generate everything from source, then plot
#   ./run.sh figure-06            one target
#
# There is one configuration and it is the paper's.  Nothing here trades
# fidelity for speed; --jobs only changes how many cores are used.
#
# Anything after the target name is passed straight through to that target's
# generate.py; run `./run.sh figure-06 --help` for the full option list.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON="${PYTHON:-python3}"

ALL_TARGETS=(figure-06 figure-07 figure-08 figure-09 figure-10
             figure-12 figure-13 figure-14 table-03 table-04)

usage() {
    cat <<EOF
HEIMDALLR artifact reproduction

Usage: ./run.sh <target|all> [options passed to the target]

Targets:
EOF
    for t in "${ALL_TARGETS[@]}"; do
        desc=$("$PYTHON" - "$ROOT/targets/$t/generate.py" <<'PY'
import ast, sys
doc = ast.get_docstring(ast.parse(open(sys.argv[1]).read())) or ""
print(doc.splitlines()[0] if doc else "")
PY
)
        printf '  %-12s %s\n' "$t" "$desc"
    done
    cat <<EOF

Every target generates its data from source using the paper's parameters
(10,000 Monte-Carlo runs per grid point over a 30-day horizon; 100 s recorded
per benchmark configuration, 200 s for PISTIS).  These are not configurable.

Options:
  --jobs N           parallel workers (speed only, never results)
  --force            regenerate even if a cached result exists
  --format pdf,png   output formats
  --no-stamp         omit the parameter stamp in the figure corner

Outputs land in outputs/.  See README.md for what each target costs.
Run ./compare.py afterwards to check the generated data against the paper's.
EOF
}

if [[ $# -eq 0 || "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

target="$1"; shift

run_one() {
    local t="$1"; shift
    if [[ ! -f "$ROOT/targets/$t/generate.py" ]]; then
        echo "unknown target: $t" >&2
        echo "try one of: ${ALL_TARGETS[*]}" >&2
        return 2
    fi
    "$PYTHON" "$ROOT/targets/$t/generate.py" "$@"
}

if [[ "$target" == "all" ]]; then
    failed=()
    for t in "${ALL_TARGETS[@]}"; do
        echo
        if ! run_one "$t" "$@"; then
            failed+=("$t")
            echo "!! $t failed" >&2
        fi
    done
    echo
    if [[ ${#failed[@]} -gt 0 ]]; then
        echo "failed targets: ${failed[*]}" >&2
        exit 1
    fi
    echo "All targets done.  Outputs in $ROOT/outputs/"
else
    run_one "$target" "$@"
fi
