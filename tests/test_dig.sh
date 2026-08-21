#!/usr/bin/env bash
#
# test_dig.sh — Test suite for dig command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── dig ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' dig --help

echo "  ── --version shows modbox version ──"
assert_cmd_pat 'modbox' dig --version

echo "  ── no args is a usage error (exit 2) ──"
"$MODBOX" dig >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "dig (no args) → exit 2"; else fail "dig (no args) → exit $rc, expected 2"; fi

echo "  ── -h shows help ──"
assert_cmd_pat 'Usage:' dig -h

echo "  ── -V shows version ──"
assert_cmd_pat 'modbox' dig -V

# ── Discoverability ──
echo "  ── dig appears in help listing ──"
assert_cmd_pat 'dig' help

