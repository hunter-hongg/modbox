#!/usr/bin/env bash
#
# test_tcpdump.sh — Test suite for tcpdump command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tcpdump ─────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' tcpdump --help

echo "  ── --version shows modbox version ──"
assert_cmd_pat 'modbox' tcpdump --version

echo "  ── -V shows modbox version ──"
assert_cmd_pat 'modbox' tcpdump -V

echo "  ── tcpdump appears in help listing ──"
assert_cmd_pat 'tcpdump' help

echo "  ── unknown option is a usage error (exit 2) ──"
"$MODBOX" tcpdump --bogus >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "tcpdump --bogus → exit 2"; else fail "tcpdump --bogus → exit $rc, expected 2"; fi

echo "  ── no args prints not-yet-implemented stub (exit 1) ──"
assert_cmd_pat_stderr 'not yet implemented' tcpdump
"$MODBOX" tcpdump >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "tcpdump (no args) → exit 1"; else fail "tcpdump (no args) → exit $rc, expected 1"; fi

echo "  ── -h shows usage ──"
assert_cmd_pat 'Usage:' tcpdump -h
