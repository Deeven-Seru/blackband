#!/bin/bash
# test.sh - AgentC compiler test suite
# Builds the compiler (if not already built) and compiles each example program,
# verifying compile-time proofs and runtime outputs.

set -euo pipefail

# ── Paths ─────────────────────────────────────────────────────────────────────
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTC_DIR="$REPO_ROOT/agentc"
BUILD_DIR="$AGENTC_DIR/build"
EXAMPLES_DIR="$AGENTC_DIR/examples"
RESULTS_FILE="$BUILD_DIR/test_results.txt"
COMPILER="$BUILD_DIR/agentc"

# ── Colour helpers ─────────────────────────────────────────────────────────────
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass()    { echo -e "${GREEN}  ✓ $*${NC}"; }
fail()    { echo -e "${RED}  ✗ $*${NC}"; }
info()    { echo -e "${YELLOW}  » $*${NC}"; }

# ── Counters ──────────────────────────────────────────────────────────────────
TOTAL=0
PASSED=0
FAILED=0
EXPECTED_FAIL=0

# ── Step 1: Ensure compiler is built ─────────────────────────────────────────
echo ""
echo "========================================"
echo "  AgentC Build & Test Suite"
echo "========================================"
echo ""

if [ ! -f "$COMPILER" ]; then
    info "Compiler not found — building now..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -5
    ninja -j"$(nproc)" 2>&1 | tail -5
    echo ""
fi

if [ ! -f "$COMPILER" ]; then
    fail "Build failed: agentc binary not found at $COMPILER"
    exit 1
fi
pass "Compiler ready: $COMPILER"
echo ""

# ── Helper: compile a program that SHOULD succeed ─────────────────────────────
compile_ok() {
    local src="$1"
    local out="$2"
    local label="$(basename "$src")"
    TOTAL=$((TOTAL + 1))
    local tmpout
    tmpout="$(mktemp)"
    if "$COMPILER" "$src" -o "$out" >"$tmpout" 2>&1; then
        pass "[COMPILE] $label → $(basename "$out") ✓"
        PASSED=$((PASSED + 1))
        rm -f "$tmpout"
        return 0
    else
        fail "[COMPILE] $label → FAILED (unexpected)"
        cat "$tmpout"
        FAILED=$((FAILED + 1))
        rm -f "$tmpout"
        return 1
    fi
}

# ── Helper: compile a program that SHOULD fail ───────────────────────────────
compile_fail() {
    local src="$1"
    local label="$(basename "$src")"
    TOTAL=$((TOTAL + 1))
    local tmpout
    tmpout="$(mktemp)"
    if "$COMPILER" "$src" -o /dev/null >"$tmpout" 2>&1; then
        fail "[PROOF]   $label → compiled (expected failure!)"
        FAILED=$((FAILED + 1))
        rm -f "$tmpout"
        return 1
    else
        pass "[PROOF]   $label → FAILED (expected) ✓"
        EXPECTED_FAIL=$((EXPECTED_FAIL + 1))
        PASSED=$((PASSED + 1))
        rm -f "$tmpout"
        return 0
    fi
}

# ── Step 2: Compile programs that should succeed ──────────────────────────────
echo "[TEST GROUP 1] Programs that should compile successfully"
echo "--------------------------------------------------------"

compile_ok "$EXAMPLES_DIR/hello.agc"                  "$BUILD_DIR/hello_bin"
compile_ok "$EXAMPLES_DIR/math.agc"                   "$BUILD_DIR/math_bin"
compile_ok "$EXAMPLES_DIR/state_machine_agent.agc"    "$BUILD_DIR/state_bin"

echo ""
echo "[TEST GROUP 2] Security proofs (programs that must NOT compile)"
echo "---------------------------------------------------------------"

compile_fail "$EXAMPLES_DIR/proof_immutability.agc"
compile_fail "$EXAMPLES_DIR/proof_trust.agc"

# ── Step 3: Run unit tests ────────────────────────────────────────────────────
echo ""
echo "[TEST GROUP 3] Unit tests"
echo "-------------------------"
TOTAL=$((TOTAL + 1))
if [ -f "$BUILD_DIR/agentc_tests" ]; then
    if "$BUILD_DIR/agentc_tests" >/dev/null 2>&1; then
        pass "[UNIT] agentc_tests → all passed ✓"
        PASSED=$((PASSED + 1))
    else
        fail "[UNIT] agentc_tests → FAILED"
        "$BUILD_DIR/agentc_tests" 2>&1 | tail -20
        FAILED=$((FAILED + 1))
    fi
else
    info "[UNIT] agentc_tests binary not found — skipping"
    TOTAL=$((TOTAL - 1))
fi

# ── Step 4: Summary ──────────────────────────────────────────────────────────
echo ""
echo "========================================"
echo "  RESULTS"
echo "========================================"
echo "  Total:            $TOTAL"
echo "  Passed:           $PASSED"
echo "  Failed:           $FAILED"
echo "  Expected failures:$EXPECTED_FAIL"
echo ""

# Write machine-readable results
mkdir -p "$BUILD_DIR"
cat >"$RESULTS_FILE" <<EOF
total=$TOTAL
passed=$PASSED
failed=$FAILED
expected_fail=$EXPECTED_FAIL
EOF

if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}  All tests passing — Build successful ✓${NC}"
    echo ""
    exit 0
else
    echo -e "${RED}  $FAILED unexpected failure(s) — Build FAILED ✗${NC}"
    echo ""
    exit 1
fi
