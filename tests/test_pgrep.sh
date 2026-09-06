#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── pgrep ──────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' pgrep --help

echo "  ── -h short form ──"
assert_cmd_pat 'Usage:' pgrep -h

echo "  ── --version ──"
assert_cmd_pat 'pgrep' pgrep --version

echo "  ── -x finds exact comm match ──"
(
  sleep 99999 &
  SLEEP_PID=$!
  sleep 0.1
  assert_cmd_pat "$SLEEP_PID" pgrep -x sleep
  kill $SLEEP_PID 2>/dev/null || true
  wait $SLEEP_PID 2>/dev/null || true
)

echo "  ── -l lists PID and name ──"
(
  sleep 99998 &
  SLEEP_PID=$!
  sleep 0.1
  assert_cmd_pat "$SLEEP_PID sleep" pgrep -l sleep
  kill $SLEEP_PID 2>/dev/null || true
  wait $SLEEP_PID 2>/dev/null || true
)

echo "  ── -a lists PID and full cmdline ──"
(
  sleep 99997 &
  SLEEP_PID=$!
  sleep 0.1
  assert_cmd_pat 'sleep 99997' pgrep -a sleep
  kill $SLEEP_PID 2>/dev/null || true
  wait $SLEEP_PID 2>/dev/null || true
)

echo "  ── -f matches full command line ──"
(
  sleep 88888 &
  SLEEP_PID=$!
  sleep 0.1
  assert_cmd_pat "$SLEEP_PID" pgrep -f 'sleep 88888'
  kill $SLEEP_PID 2>/dev/null || true
  wait $SLEEP_PID 2>/dev/null || true
)

echo "  ── -i is case-insensitive ──"
(
  sleep 77777 &
  SLEEP_PID=$!
  sleep 0.1
  assert_cmd_pat "$SLEEP_PID" pgrep -i SLEEP
  kill $SLEEP_PID 2>/dev/null || true
  wait $SLEEP_PID 2>/dev/null || true
)

echo "  ── -f falls back to comm for kernel threads ──"
assert_cmd_pat '^2 kthreadd$' pgrep -a -f kthreadd

echo "  ── -v inverts match ──"
(
  assert_cmd_not_pat '^$$' pgrep -v zzzz_not_exist_xyz_12345
)

echo "  ── -c counts matches ──"
(
  sleep 66666 &
  PID1=$!
  sleep 66665 &
  PID2=$!
  sleep 0.1
  COUNT=$("$MODBOX" pgrep -c sleep)
  if [ "$COUNT" -ge 2 ]; then
    pass "pgrep -c sleep >= 2 (got $COUNT)"
  else
    fail "pgrep -c sleep expected >=2 got $COUNT"
  fi
  kill $PID1 $PID2 2>/dev/null || true
  wait $PID1 $PID2 2>/dev/null || true
)

echo "  ── -u user filter ──"
(
  sleep 55555 &
  SLEEP_PID=$!
  sleep 0.1
  OUTPUT=$("$MODBOX" pgrep -u "$(id -un)" -x sleep)
  if echo "$OUTPUT" | grep -q "$SLEEP_PID"; then
    pass "pgrep -u $(id -un) -x sleep → finds own sleep"
  else
    fail "pgrep -u $(id -un) -x sleep → expected $SLEEP_PID got [$OUTPUT]"
  fi
  kill $SLEEP_PID 2>/dev/null || true
  wait $SLEEP_PID 2>/dev/null || true
)

echo "  ── -u invalid user exits 2 ──"
"$MODBOX" pgrep -u definitely_no_such_user_xyz zzz 2>/dev/null
if [ "$?" -eq 2 ]; then
  pass "pgrep -u invalid user → exit 2"
else
  fail "pgrep -u invalid user → expected exit 2"
fi

echo "  ── no match exits 1 ──"
"$MODBOX" pgrep -x zzz_no_such_comm_xyz 2>/dev/null
if [ "$?" -eq 1 ]; then
  pass "pgrep no match → exit 1"
else
  fail "pgrep no match → expected exit 1"
fi

echo "  ── missing operand errors ──"
assert_cmd_pat_stderr 'missing required operand' pgrep

echo "  ── duplicate pattern errors ──"
assert_cmd_pat_stderr 'only one pattern' pgrep sleep sleep

echo "  ── unrecognized option errors ──"
assert_cmd_pat_stderr 'unrecognized option' pgrep --definitely-not-an-option
