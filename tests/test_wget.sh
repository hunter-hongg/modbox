#!/usr/bin/env bash
# Test suite for wget command
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "-- wget ---------------------------------------"

# ── Argument validation (no network required) ──
echo " -- --help --"
assert_cmd_pat 'Usage: wget' wget --help
"$MODBOX" wget --help 2>&1 | head -1 | grep -q "wget (modbox)" && pass "wget --help exits 0" || fail "wget --help failed"

echo " -- --version --"
assert_cmd_pat 'wget \(modbox\) 1.0' wget --version
"$MODBOX" wget --version 2>&1 | grep -q "wget (modbox) 1.0" && pass "wget --version exits 0" || fail "wget --version failed"

echo " -- no URL --"
assert_cmd_pat_stderr 'missing URL' wget
"$MODBOX" wget 2>/dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "wget no URL exits non-zero"
else
    fail "wget no URL should exit non-zero"
fi

echo " -- unknown option --"
assert_cmd_pat_stderr 'invalid option' wget --foo

echo " -- invalid URL --"
assert_cmd_pat_stderr 'failed to parse URL' wget '://bad' 2>/dev/null || true
# --output-document and --output-file conflict --
assert_cmd_pat_stderr 'conflict' wget -O out.txt -o log.txt http://127.0.0.1:39059/ 2>/dev/null || true
# --no-clobber and --continue conflict
assert_cmd_pat_stderr 'conflict' wget -nc -c http://127.0.0.1:39059/ 2>/dev/null || true
# --background and --output-file conflict
assert_cmd_pat_stderr 'conflict' wget -b -o log.txt http://127.0.0.1:39059/ 2>/dev/null || true

echo " -- no args --"
"$MODBOX" wget 2>/dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "wget with no args exits non-zero"
else
    fail "wget with no args should exit non-zero"
fi

echo " -- summary --"
FAIL_COUNT=$("$SCRIPT_DIR/framework.sh" -count)
echo "  FAIL=$FAIL_COUNT PASS=$(("$PASS_COUNT - $FAIL_COUNT"))"
if [[ $FAIL_COUNT -gt 0 ]]; then
    echo "  Some wget tests FAILED"
    exit 1
fi
echo "  All wget tests passed"
exit 0
