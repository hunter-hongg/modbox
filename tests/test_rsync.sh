#!/usr/bin/env bash
# Test suite for rsync command
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "-- rsync --------------------------------------"

# ── Argument validation (no filesystem mutation required) ──
echo " -- --help --"
assert_cmd_pat 'Usage: rsync' rsync --help
"$MODBOX" rsync --help 2>&1 | head -1 | grep -q "rsync (modbox)" && pass "rsync --help exits 0" || fail "rsync --help failed"

echo " -- --version --"
assert_cmd_pat 'rsync \(modbox\) 1.0' rsync --version
"$MODBOX" rsync --version 2>&1 | grep -q "rsync (modbox) 1.0" && pass "rsync --version exits 0" || fail "rsync --version failed"

echo " -- no args --"
assert_cmd_pat_stderr 'specify at least one source' rsync
"$MODBOX" rsync 2>/dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "rsync no args exits non-zero"
else
    fail "rsync no args should exit non-zero"
fi

echo " -- one arg only --"
assert_cmd_pat_stderr 'specify at least one source' rsync /tmp/src
"$MODBOX" rsync /tmp/src 2>/dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "rsync one arg exits non-zero"
else
    fail "rsync one arg should exit non-zero"
fi

echo " -- unknown option --"
assert_cmd_pat_stderr 'invalid option' rsync --foo /tmp/a /tmp/b

echo " -- --progress and --quiet conflict --"
assert_cmd_pat_stderr 'conflict' rsync --progress --quiet /tmp/src /tmp/dst 2>/dev/null || true

echo " -- summary --"
FAIL_COUNT=$("$SCRIPT_DIR/framework.sh" -count)
echo "  FAIL=$FAIL_COUNT PASS=$(("$PASS_COUNT - $FAIL_COUNT"))"
if [[ $FAIL_COUNT -gt 0 ]]; then
    echo "  Some rsync tests FAILED"
    exit 1
fi
echo "  All rsync tests passed"
exit 0
