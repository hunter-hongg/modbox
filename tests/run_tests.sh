#!/usr/bin/env bash
#
# run_tests.sh — Automated test suite for modbox
#
# Sources the shared framework, then runs each test_*.sh file.
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source framework (defines MODBOX, TMPDIR, helpers, counters)
source "$SCRIPT_DIR/framework.sh"

echo "============================================"
echo "  modbox Test Suite"
echo "  Binary: $MODBOX"
echo "============================================"
echo ""

# Run each test file in sorted order
for test_file in "$SCRIPT_DIR"/test_*.sh; do
    source "$test_file"
done

# ── Summary ─────────────────────────────────────────────────────────────────

echo ""
echo "════════════════════════════════════════════"
echo "  Results: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "════════════════════════════════════════════"

if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi
exit 0
