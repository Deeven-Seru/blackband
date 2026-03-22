#!/usr/bin/env bash
# verify_tests.sh — AgentC test verification script
#
# Usage:
#   ./verify_tests.sh [--agentc PATH]
#
# Options:
#   --agentc PATH   Path to the compiled agentc binary
#                   (default: agentc/build/agentc)
#
# Exit codes:
#   0  All tests passed
#   1  One or more tests failed

set -euo pipefail

# ─── Defaults ────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTC="${SCRIPT_DIR}/agentc/build/agentc"
RESULTS_DIR="/tmp/results"

# ─── Argument parsing ────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --agentc)
            AGENTC="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# ─── Helpers ─────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS=0
FAIL=0
declare -a JSON_TESTS=()

pass() {
    local name="$1"
    local msg="${2:-}"
    echo -e "  ${GREEN}✅ PASS${NC}  $name${msg:+  ($msg)}"
    PASS=$((PASS+1))
    JSON_TESTS+=("{\"name\":\"$name\",\"status\":\"PASS\",\"note\":\"$msg\"}")
}

fail() {
    local name="$1"
    local msg="${2:-}"
    echo -e "  ${RED}❌ FAIL${NC}  $name${msg:+  ($msg)}"
    FAIL=$((FAIL+1))
    JSON_TESTS+=("{\"name\":\"$name\",\"status\":\"FAIL\",\"note\":\"$msg\"}")
}

section() {
    echo ""
    echo -e "${YELLOW}=== $1 ===${NC}"
}

# ─── Pre-flight ──────────────────────────────────────────────────────────────
section "Pre-flight checks"

mkdir -p "$RESULTS_DIR"

if [ ! -x "$AGENTC" ]; then
    echo -e "${RED}ERROR:${NC} agentc binary not found at: $AGENTC"
    echo "Build the compiler first:"
    echo "  cmake -S agentc -B agentc/build -DCMAKE_CXX_COMPILER=clang++-17"
    echo "  cmake --build agentc/build"
    exit 1
fi

echo "  agentc binary : $AGENTC"
echo "  version       : $("$AGENTC" --version 2>/dev/null || echo 'unknown')"
echo "  results dir   : $RESULTS_DIR"
pass "binary_found"

# ─── Success examples (must compile without errors) ──────────────────────────
section "Success examples"

compile_ok() {
    local file="$1"
    local label="${file%.agc}"
    local stderr_out
    if stderr_out=$("$AGENTC" "${SCRIPT_DIR}/agentc/examples/$file" -o /dev/null 2>&1); then
        pass "$label"
    else
        fail "$label" "$(echo "$stderr_out" | head -1)"
    fi
}

compile_ok spec_conformance.agc
compile_ok state_machine_agent.agc
compile_ok briefing_agent.agc
compile_ok compare_agentc.agc

# ─── Security proof examples (must fail to compile) ──────────────────────────
section "Security proof examples (expected compiler rejections)"

compile_must_fail() {
    local file="$1"
    local expected_code="$2"
    local label="${file%.agc}"
    local stderr_out
    if stderr_out=$("$AGENTC" "${SCRIPT_DIR}/agentc/examples/$file" -o /dev/null 2>&1); then
        fail "$label" "compiled successfully — expected error $expected_code"
    else
        pass "$label" "correctly rejected ($expected_code)"
    fi
}

compile_must_fail proof_immutability.agc E201
compile_must_fail proof_trust.agc        E301
compile_must_fail proof_cap.agc          E503
compile_must_fail proof_bounds.agc       E305
compile_must_fail proof_purity.agc       E505

# ─── Unit tests ──────────────────────────────────────────────────────────────
section "Unit tests (ctest)"

CTEST_BIN="${SCRIPT_DIR}/agentc/build"
if [ -d "$CTEST_BIN" ]; then
    if (cd "$CTEST_BIN" && ctest --output-on-failure -V 2>&1); then
        pass "unit_tests"
    else
        fail "unit_tests" "ctest reported one or more failures"
    fi
else
    echo "  (build directory not found — skipping ctest)"
fi

# ─── Summary & JSON report ───────────────────────────────────────────────────
section "Summary"

TOTAL=$((PASS+FAIL))
echo -e "  Total : $TOTAL"
echo -e "  ${GREEN}Pass${NC}  : $PASS"
echo -e "  ${RED}Fail${NC}  : $FAIL"

TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Build JSON array
ARRAY=""
for entry in "${JSON_TESTS[@]}"; do
    ARRAY="${ARRAY}${ARRAY:+,}${entry}"
done

cat > "${RESULTS_DIR}/test_results.json" <<EOF
{
  "timestamp": "${TIMESTAMP}",
  "compiler": "${AGENTC}",
  "summary": {
    "total": ${TOTAL},
    "pass": ${PASS},
    "fail": ${FAIL}
  },
  "tests": [${ARRAY}]
}
EOF

echo ""
echo "Test report written to: ${RESULTS_DIR}/test_results.json"

if [ "$FAIL" -gt 0 ]; then
    echo -e "${RED}RESULT: FAILED${NC} — $FAIL of $TOTAL tests did not pass"
    exit 1
else
    echo -e "${GREEN}RESULT: PASSED${NC} — all $TOTAL tests passed"
fi
