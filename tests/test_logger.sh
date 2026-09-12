SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── logger ────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage: logger' logger --help
assert_cmd_pat 'facility' logger --help
assert_cmd_pat 'severity' logger --help

echo "  ── --version / -V ──"
assert_cmd_pat 'logger \(modbox\) 1\.0' logger --version
assert_cmd_pat 'logger \(modbox\) 1\.0' logger -V

echo "  ── help logger is discoverable ──"
assert_cmd_pat 'logger' help logger
assert_cmd_pat 'logger' help

echo "  ── -s echoes the message to stderr ──"
assert_cmd_pat_stderr 'hello stderr' logger -s 'hello stderr'

echo "  ── -s joins multiple message arguments with spaces ──"
assert_cmd_pat_stderr 'one two three' logger -s one two three

echo "  ── no -s keeps stdout clean ──"
assert_cmd_not_pat 'quiet message' logger 'quiet message'

echo "  ── unrecognized option exits non-zero with argtable3 wording ──"
assert_cmd_pat_stderr "unrecognized option '--nope'" logger --nope
"$MODBOX" logger --nope >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "logger --nope → non-zero exit ($rc)"; else fail "logger --nope → expected non-zero, got 0"; fi

echo "  ── -p accepts facility.severity and bare severity ──"
"$MODBOX" logger -s -p local0.debug 'pri ok' >/dev/null 2>&1; rc=$?
if [[ $rc -eq 0 ]]; then pass "logger -p local0.debug → exit 0"; else fail "logger -p local0.debug → expected 0, got $rc"; fi
"$MODBOX" logger -s -p info 'pri ok' >/dev/null 2>&1; rc=$?
if [[ $rc -eq 0 ]]; then pass "logger -p info → exit 0"; else fail "logger -p info → expected 0, got $rc"; fi

echo "  ── unknown priority is rejected ──"
assert_cmd_pat_stderr 'unknown facility/priority' logger -p bogus.sev 'x'
"$MODBOX" logger -p bogus.sev x >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "logger -p bogus.sev → non-zero exit ($rc)"; else fail "logger -p bogus.sev → expected non-zero, got 0"; fi

echo "  ── -f logs file contents (trailing newline stripped) ──"
MSGFILE="$TMPDIR/logger_msg.txt"
printf 'from a file\n' > "$MSGFILE"
assert_cmd_pat_stderr 'from a file' logger -s -f "$MSGFILE"
assert_cmd_not_pat 'from a file' logger -f "$MSGFILE"

echo "  ── -f - reads stdin ──"
STDIN_OUT=$(printf 'piped in\n' | "$MODBOX" logger -s -f - 2>&1)
if [[ "$STDIN_OUT" == "piped in" ]]; then pass "logger -f - reads stdin"; else fail "logger -f - expected [piped in], got [$STDIN_OUT]"; fi

echo "  ── stdin is used when no message argument is given ──"
STDIN_OUT=$(printf 'stdin body\n' | "$MODBOX" logger -s 2>&1)
if [[ "$STDIN_OUT" == "stdin body" ]]; then pass "logger reads stdin by default"; else fail "logger stdin expected [stdin body], got [$STDIN_OUT]"; fi

echo "  ── -f with a nonexistent file fails ──"
assert_cmd_pat_stderr 'No such file' logger -f "$TMPDIR/does_not_exist"
"$MODBOX" logger -f "$TMPDIR/does_not_exist" >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "logger -f missing → non-zero exit ($rc)"; else fail "logger -f missing → expected non-zero, got 0"; fi

echo "  ── --file plus a message argument is rejected ──"
assert_cmd_pat_stderr 'mutually exclusive' logger -f "$MSGFILE" 'extra'
"$MODBOX" logger -f "$MSGFILE" extra >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "logger -f FILE MSG → non-zero exit ($rc)"; else fail "logger -f FILE MSG → expected non-zero, got 0"; fi
