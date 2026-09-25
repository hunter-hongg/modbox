#!/usr/bin/env bash
#
# test_pidof.sh — Tests for the pidof command.
#

# shellcheck source=framework.sh
source "$(dirname "${BASH_SOURCE[0]}")/framework.sh"

echo ""
echo "── pidof ──────────────────────────────────────"

# Distinctive unique marker so parallel test runs cannot collide with
# another suite's processes.
MARK=zzpidofmarker

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' pidof --help

echo "  ── -h short form ──"
assert_cmd_pat 'Usage:' pidof -h

echo "  ── -V version ──"
assert_cmd_pat 'pidof \(modbox\)' pidof -V

echo "  ── finds a running program by name ──"
(
  sleep 98765 &
  PID1=$!
  sleep 0.1
  assert_cmd_pat "$PID1" pidof sleep
  kill $PID1 2>/dev/null || true
  wait $PID1 2>/dev/null || true
)

echo "  ── no match prints nothing and exits 1 ──"
(
  OUT=$("$MODBOX" pidof ${MARK}_nope 2>/dev/null)
  RC=$?
  if [ $RC -eq 1 ] && [ -z "$OUT" ]; then
    pass "pidof <no match> → empty output, exit 1"
  else
    fail "pidof <no match> → expected empty + exit 1, got [$OUT] + exit $RC"
  fi
)

echo "  ── multiple PIDs listed highest first, space separated ──"
(
  sleep 98760 & P1=$!
  sleep 98759 & P2=$!
  sleep 98758 & P3=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof sleep)
  # Extract only our own PIDs, preserving the order pidof emitted them.
  FOUND=""
  for p in $OUT; do
    if [ "$p" = "$P1" ] || [ "$p" = "$P2" ] || [ "$p" = "$P3" ]; then
      FOUND="$FOUND $p"
    fi
  done
  FOUND_T=$(echo $FOUND)
  EXPECTED_T=$(printf '%s\n' $P1 $P2 $P3 | sort -rn | tr '\n' ' ' | sed 's/ *$//')
  if [ -n "$FOUND_T" ] && [ "$FOUND_T" = "$EXPECTED_T" ]; then
    pass "pidof sleep → $FOUND_T descending"
  else
    fail "pidof sleep → expected descending [$EXPECTED_T] got [$FOUND_T] (full: $OUT)"
  fi
  kill $P1 $P2 $P3 2>/dev/null || true
  wait $P1 $P2 $P3 2>/dev/null || true
)

echo "  ── multiple PROGRAM arguments match the union ──"
(
  sleep 98750 & PS=$!
  sleep 98749 & PS2=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof sleep ${MARK}_nope 2>/dev/null)
  if echo " $OUT " | grep -q " $PS " && echo " $OUT " | grep -q " $PS2 "; then
    pass "pidof sleep <no-match> → still lists sleep PIDs"
  else
    fail "pidof sleep <no-match> → expected $PS/$PS2 in [$OUT]"
  fi
  kill $PS $PS2 2>/dev/null || true
  wait $PS $PS2 2>/dev/null || true
)

echo "  ── -s single-shot returns the highest PID only ──"
(
  # Use a private copy of sleep so concurrent suites' `sleep` processes
  # cannot make the global-highest assertion racy: only our two PIDs can
  # ever match this name.
  cp "$(command -v sleep)" "$TMPDIR/${MARK}s1"
  "$TMPDIR/${MARK}s1" 98740 & P1=$!
  "$TMPDIR/${MARK}s1" 98739 & P2=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof -s "${MARK}s1")
  HIGHEST=$(( P1 > P2 ? P1 : P2 ))
  if [ "$OUT" = "$HIGHEST" ]; then
    pass "pidof -s ${MARK}s1 → $OUT (highest of $P1/$P2)"
  else
    fail "pidof -s ${MARK}s1 → expected $HIGHEST got [$OUT]"
  fi
  kill $P1 $P2 2>/dev/null || true
  wait $P1 $P2 2>/dev/null || true
  rm -f "$TMPDIR/${MARK}s1"
)

echo "  ── -o omits PIDs (repeated form) ──"
(
  sleep 98730 & P1=$!
  sleep 98729 & P2=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof -o $P1 sleep)
  if ! echo " $OUT " | grep -q " $P1 "; then
    pass "pidof -o $P1 → $P1 absent"
  else
    fail "pidof -o $P1 → $P1 still present in [$OUT]"
  fi
  OUT2=$("$MODBOX" pidof -o $P1 -o $P2 sleep)
  if ! echo " $OUT2 " | grep -q " $P1 " && ! echo " $OUT2 " | grep -q " $P2 "; then
    pass "pidof -o $P1 -o $P2 → both absent"
  else
    fail "pidof -o repeated → [$OUT2] still contains an omitted PID"
  fi
  kill $P1 $P2 2>/dev/null || true
  wait $P1 $P2 2>/dev/null || true
)

echo "  ── -o accepts a comma-separated list ──"
(
  sleep 98720 & P1=$!
  sleep 98719 & P2=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof -o $P1,$P2 sleep)
  if ! echo " $OUT " | grep -q " $P1 " && ! echo " $OUT " | grep -q " $P2 "; then
    pass "pidof -o $P1,$P2 → both absent"
  else
    fail "pidof -o list → [$OUT] still contains an omitted PID"
  fi
  kill $P1 $P2 2>/dev/null || true
  wait $P1 $P2 2>/dev/null || true
)

echo "  ── omitting the only match yields empty output and exit 1 ──"
(
  # A uniquely named private copy of sleep guarantees exactly one match.
  cp "$(command -v sleep)" "$TMPDIR/${MARK}bin"
  "$TMPDIR/${MARK}bin" 5 &
  ONLY=$!
  sleep 0.1
  BEFORE=$("$MODBOX" pidof "${MARK}bin" 2>/dev/null)
  OUT=$("$MODBOX" pidof -o $ONLY "${MARK}bin" 2>/dev/null)
  RC=$?
  if [ "$BEFORE" = "$ONLY" ] && [ -z "$OUT" ] && [ $RC -eq 1 ]; then
    pass "pidof -o <only-pid> → empty output, exit 1"
  else
    fail "pidof -o <only-pid> → before=[$BEFORE] after=[$OUT] exit=$RC"
  fi
  kill $ONLY 2>/dev/null || true
  wait $ONLY 2>/dev/null || true
)

echo "  ── -o %PPID omits the caller's parent process ──"
(
  SUB=$BASHPID   # pid of this subshell = PPID of the backgrounded pidof
  "$MODBOX" pidof -o %PPID bash > "$TMPDIR/ppid_omit.txt" 2>/dev/null &
  wait $!
  "$MODBOX" pidof bash > "$TMPDIR/ppid_ctrl.txt" 2>/dev/null &
  wait $!
  CTRL=$(cat "$TMPDIR/ppid_ctrl.txt")
  OMIT=$(cat "$TMPDIR/ppid_omit.txt")
  if echo " $CTRL " | grep -q " $SUB " && ! echo " $OMIT " | grep -q " $SUB "; then
    pass "pidof -o %PPID → parent $SUB listed without -o, hidden with -o"
  else
    fail "pidof -o %PPID → control=[$CTRL] omit=[$OMIT], expected $SUB in control only"
  fi
)

echo "  ── -S custom separator ──"
(
  sleep 98690 & P1=$!
  sleep 98689 & P2=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof -S, sleep)
  if [[ "$OUT" =~ ^[0-9]+(,[0-9]+)+$ ]] && echo "$OUT" | grep -q "$P1"; then
    pass "pidof -S, sleep → comma-separated [$OUT]"
  else
    fail "pidof -S, sleep → expected comma-joined PIDs got [$OUT]"
  fi
  kill $P1 $P2 2>/dev/null || true
  wait $P1 $P2 2>/dev/null || true
)

echo "  ── -q quiet mode prints nothing but keeps the exit code ──"
(
  sleep 98680 & PS=$!
  sleep 0.1
  OUT=$("$MODBOX" pidof -q sleep)
  RC=$?
  if [ -z "$OUT" ] && [ $RC -eq 0 ]; then
    pass "pidof -q sleep → silent, exit 0"
  else
    fail "pidof -q sleep → expected silent+0, got [$OUT]+exit $RC"
  fi
  kill $PS 2>/dev/null || true
  wait $PS 2>/dev/null || true
)
(
  OUT=$("$MODBOX" pidof -q ${MARK}_nope 2>/dev/null)
  RC=$?
  if [ -z "$OUT" ] && [ $RC -eq 1 ]; then
    pass "pidof -q <no match> → silent, exit 1"
  else
    fail "pidof -q <no match> → expected silent+1, got [$OUT]+exit $RC"
  fi
)

echo "  ── -x finds a shell running the named script ──"
(
  # Wrapper runs with no stdout/stderr (the test's pipe fds must not leak
  # into the sleep child and hold the orchestrator's pipe open) and a
  # bounded sleep, so even a cleanup miss cannot outlive the assertion.
  cat > "$TMPDIR/${MARK}.sh" <<EOS
#!/usr/bin/env bash
sleep 59 &
wait \$!
EOS
  chmod +x "$TMPDIR/${MARK}.sh"
  bash "$TMPDIR/${MARK}.sh" > /dev/null 2>&1 &
  SCRIPT_PID=$!
  sleep 0.2
  OUT=$("$MODBOX" pidof -x "${MARK}.sh" 2>/dev/null)
  if echo " $OUT " | grep -q " $SCRIPT_PID "; then
    pass "pidof -x ${MARK}.sh → finds wrapper shell $SCRIPT_PID"
  else
    fail "pidof -x ${MARK}.sh → expected $SCRIPT_PID in [$OUT]"
  fi
  # Without -x the shell (comm=bash) must NOT be found under the script name.
  OUT2=$("$MODBOX" pidof "${MARK}.sh" 2>/dev/null)
  if [ -z "$OUT2" ]; then
    pass "pidof ${MARK}.sh (no -x) → not found"
  else
    fail "pidof ${MARK}.sh (no -x) → expected empty got [$OUT2]"
  fi
  # Deterministic teardown: kill the wrapper and its child, then reap both.
  # Capture the child pid BEFORE the wrapper is reaped — once it dies, the
  # child re-parents to init and `pgrep -P <wrapper>` can no longer find it.
  KID=$(pgrep -P $SCRIPT_PID 2>/dev/null || true)
  kill $SCRIPT_PID 2>/dev/null || true
  wait $SCRIPT_PID 2>/dev/null || true
  [ -n "$KID" ] && { kill $KID 2>/dev/null || true; wait $KID 2>/dev/null || true; }
)

echo "  ── never lists itself ──"
(
  # exec makes the pidof process inherit a known PID; matching by the full
  # modbox path would otherwise list the pidof process itself.
  bash -c 'exec "$1" pidof "$1" > "$2/self.txt" 2>/dev/null' _ "$MODBOX" "$TMPDIR" &
  MP=$!
  wait $MP 2>/dev/null || true
  if [ -f "$TMPDIR/self.txt" ] && ! echo " $(cat "$TMPDIR/self.txt") " | grep -q " $MP "; then
    pass "pidof modbox → self $MP excluded"
  else
    fail "pidof modbox → self $MP present in [$(cat "$TMPDIR/self.txt" 2>/dev/null)] or capture missing"
  fi
)

echo "  ── usage errors exit 1 with a hint ──"
assert_cmd_pat_stderr 'no program name' pidof
assert_cmd_pat_stderr 'Try .* --help' pidof
assert_cmd_pat_stderr 'unrecognized option' pidof --definitely-not-an-option

echo "  ── invalid --omit-pid values rejected ──"
assert_cmd_pat_stderr 'invalid omit-pid' pidof -o notanum sleep
assert_cmd_pat_stderr 'invalid omit-pid' pidof -o 0 sleep
assert_cmd_pat_stderr 'invalid omit-pid' pidof -o 99999999999999 sleep
assert_cmd_pat_stderr 'invalid omit-pid' pidof -o 5,notanum sleep
