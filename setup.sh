#!/usr/bin/env bash
# Dependency check for the HEIMDALLR artifact reproduction.
#
#   ./setup.sh            report what is present and what is missing
#   ./setup.sh --install  additionally pip install the Python requirements
#
# Every target generates its data from source, so all of this is required;
# the toolchain items are flagged per-target so you can tell what a missing
# one costs you.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON="${PYTHON:-python3}"
install=0
[[ "${1:-}" == "--install" ]] && install=1

ok()   { printf '  \033[32mok\033[0m    %s\n' "$1"; }
warn() { printf '  \033[33mwarn\033[0m  %s\n' "$1"; }
bad()  { printf '  \033[31mmiss\033[0m  %s\n' "$1"; }

missing_required=0

echo "Python"
if command -v "$PYTHON" >/dev/null 2>&1; then
    ok "$($PYTHON --version 2>&1) at $(command -v "$PYTHON")"
else
    bad "python3 not found"; missing_required=1
fi

if [[ $install -eq 1 ]]; then
    echo
    echo "Installing Python requirements"
    "$PYTHON" -m pip install -r "$ROOT/requirements.txt"
fi

echo
echo "Python packages"
for pkg in numpy pandas matplotlib; do
    if v=$("$PYTHON" -c "import $pkg; print($pkg.__version__)" 2>/dev/null); then
        ok "$pkg $v"
    else
        bad "$pkg  (pip install -r requirements.txt)"; missing_required=1
    fi
done
if v=$("$PYTHON" -c "import numba; print(numba.__version__)" 2>/dev/null); then
    ok "numba $v"
else
    warn "numba  - the TGS simulator falls back to pure Python without it,"
    warn "         which at the paper's 10,000 runs/point is not practical"
fi

echo
echo "Toolchain"
for tool in g++ make; do
    if command -v "$tool" >/dev/null 2>&1; then ok "$tool"; else
        warn "$tool  - needed to build the PISTIS simulator and the benchmarks"
    fi
done

if "$PYTHON" - <<'PY' 2>/dev/null
import ctypes.util, sys
sys.exit(0 if ctypes.util.find_library("sodium") else 1)
PY
then
    ok "libsodium"
else
    warn "libsodium  - needed for the localhost benchmarks (Figure 9, Table 4)"
    warn "             Debian/Ubuntu: sudo apt install libsodium-dev"
fi

echo
NS3="${NS3_ROOT:-$ROOT/../railway-ns3}"
if [[ -d "$NS3/build/include/ns3" ]]; then
    ok "ns-3 for Figure 10 at $NS3"
else
    warn "ns-3 not found at $NS3"
    warn "  Figure 10 will plot the recorded traces instead of regenerating them."
    warn "  It is the only target that needs anything outside this directory."
    warn "  To generate it:  NS3_ROOT=/path/to/railway-ns3 ./run.sh figure-10"
fi

echo
NCPU=$(nproc 2>/dev/null || echo 0)
echo "Cores available: ${NCPU:-?}"
if [[ "$NCPU" -gt 0 ]]; then
    # ~227 core-hours of parallel Monte-Carlo work (measured on 16 cores),
    # plus ~1.7 wall-hours of fixed recording windows and single-threaded ns-3.
    EST=$(awk -v n="$NCPU" 'BEGIN { printf "%.0f", 227.2 / n + 1.7 }')
    echo "  Estimated time for ./run.sh all on this machine: ~${EST} h"
    echo "  (227 core-hours of Monte-Carlo, which scales with cores, plus"
    echo "   ~1.7 h of fixed recording windows, which does not.  See README §2.)"
fi
if [[ "$NCPU" -gt 0 && "$NCPU" -lt 26 ]]; then
    warn "Figure 9 wants >= 26 cores"
    warn "  Its largest configuration runs 26 processes (25 replicas + 1 client);"
    warn "  with fewer they share cores and the measured CPU times inflate."
    warn "  See targets/figure-09/README.md."
else
    ok "core count is enough for Figure 9 (needs >= 26)"
fi

echo
echo "Reference data"
for d in tgs pistis protocol-logs network railway; do
    n=$(find "$ROOT/data/reference/$d" -type f 2>/dev/null | wc -l)
    if [[ "$n" -gt 0 ]]; then ok "data/reference/$d  ($n files)"; else
        bad "data/reference/$d  is empty"; missing_required=1
    fi
done

echo
if [[ $missing_required -eq 0 ]]; then
    echo "Ready.  Try:  ./run.sh all"
else
    echo "Some required pieces are missing (see 'miss' above)." >&2
    exit 1
fi
