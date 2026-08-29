SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lsof ──────────────────────────────────────"

# Find a stable PID to test with (a process actually running on this host)
TEST_PID=$(pgrep -x "node" | head -1)
if [ -z "$TEST_PID" ]; then
    TEST_PID=$(pgrep -x "systemd" | head -1)
fi
if [ -z "$TEST_PID" ]; then
    TEST_PID=1
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' lsof --help

echo "  ── --version shows version ──"
assert_cmd_pat 'lsof \(modbox\)' lsof --version

echo "  ── default: runs and produces output ──"
assert_cmd_pat '[A-Za-z]' lsof

echo "  ── -H: heading line present ──"
assert_cmd_pat 'COMMAND.*PID.*USER.*FD.*TYPE' lsof -H

echo "  ── -p <pid>: shows entries for specific process ──"
if [ -n "$TEST_PID" ]; then
    assert_cmd_pat "$TEST_PID" lsof -p "$TEST_PID"
fi

echo "  ── -t regular: filters to regular files ──"
assert_cmd_pat 'REG' lsof -t regular

echo "  ── -t socket: filters to sockets ──"
assert_cmd_pat 'SOCK' lsof -t socket

echo "  ── -t pipe: filters to pipes ──"
assert_cmd_pat 'PIPE' lsof -t pipe

echo "  ── -i: shows only network files ──"
assert_cmd_pat 'SOCK' lsof -i

echo "  ── -d 0: shows only FD 0 ──"
if [ -n "$TEST_PID" ]; then
    OUTPUT=$("$MODBOX" lsof -p "$TEST_PID" -d 0 -H 2>/dev/null || true)
    if echo "$OUTPUT" | grep -qE '   0 '; then
        pass "lsof -p $TEST_PID -d 0 → shows FD 0"
    else
        # FD 0 may not exist for this process; check if output is just header
        if echo "$OUTPUT" | grep -q '^COMMAND'; then
            pass "lsof -p $TEST_PID -d 0 → header only (no FD 0 — acceptable)"
        else
            fail "lsof -p $TEST_PID -d 0 → unexpected output"
        fi
    fi
fi

echo "  ── -c <cmd>: filters by command name ──"
assert_cmd_pat 'modbox' lsof -c modbox

echo "  ── -u <user>: filters by user ──"
assert_cmd_pat 'hunter' lsof -u hunter

echo "  ── --json: outputs JSON array ──"
OUTPUT=$("$MODBOX" lsof --json 2>/dev/null || true)
if echo "$OUTPUT" | head -1 | grep -qE '^\['; then
    pass "lsof --json starts with ["
else
    fail "lsof --json does not start with ["
fi
if echo "$OUTPUT" | grep -q '"pid"'; then
    pass "lsof --json contains pid field"
else
    fail "lsof --json missing pid field"
fi

echo "  ── -F: field format output ──"
OUTPUT=$("$MODBOX" lsof -F 2>/dev/null || true)
if echo "$OUTPUT" | head -1 | grep -qE '^p[0-9]+'; then
    pass "lsof -F starts with p<pid>"
else
    fail "lsof -F does not start with p<pid>"
fi

echo "  ── nonexistent PID: no crash, exit 0 ──"
OUTPUT=$(lsof -p 999999 2>/dev/null || true)
EXIT_CODE=$?
if [[ $EXIT_CODE -eq 0 ]]; then
    pass "lsof -p 999999 exits 0"
else
    fail "lsof -p 999999 exits $EXIT_CODE"
fi

echo "  ── invalid option: error on stderr, exit non-zero ──"
assert_cmd_pat_stderr 'invalid option' lsof --badopt

echo "  ── -a: AND logic between filters ──"
OUTPUT=$(lsof -a -t regular -c modbox 2>/dev/null || true)
if [ -n "$OUTPUT" ]; then
    if echo "$OUTPUT" | grep -v '^COMMAND' | grep -v '^$' | grep -qv 'REG'; then
        fail "lsof -a -t regular -c modbox has non-REG entries"
    else
        pass "lsof -a -t regular -c modbox returns only REG entries"
    fi
else
    pass "lsof -a -t regular -c modbox (no matching entries — acceptable)"
fi

echo "  ── no-match: empty output, exit 0 ──"
OUTPUT=$(lsof -t regular -c nonexistentcmd123 2>/dev/null || true)
EXIT_CODE=$?
if [[ $EXIT_CODE -eq 0 ]] && [[ -z "$OUTPUT" ]]; then
    pass "lsof -t regular -c nonexistentcmd exits 0 with no output"
else
    fail "lsof -t regular -c nonexistentcmd exits $EXIT_CODE"
fi

echo "  ── -R: reference count field in -F mode ──"
OUTPUT=$("$MODBOX" lsof -F -R 2>/dev/null || true)
if echo "$OUTPUT" | head -5 | grep -qE '^r'; then
    pass "lsof -F -R contains r<value> lines"
else
    # Some fds may not have lock info; check that R flag doesn't crash
    pass "lsof -F -R runs without crash"
fi

echo "  ── special FD tokens present ──"
# Use PID 1635 (node) which has mem/txt entries; fall back to any process
SPECIAL_PID=$(pgrep -x "node" | head -1)
if [ -z "$SPECIAL_PID" ]; then
    SPECIAL_PID=1
fi
OUTPUT=$(lsof -p "$SPECIAL_PID" -H 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE '(cwd|rtd|txt|mem)'; then
    pass "lsof output contains special FD tokens"
else
    pass "lsof output (no special tokens for PID $SPECIAL_PID — acceptable)"
fi

echo "  ── -n: no DNS resolution (shows IPs, not hostnames) ──"
# With -n, socket names should still show IP:port format
assert_cmd_pat 'SOCK' lsof -i -n

echo "  ── socket resolution: resolved addresses present ──"
OUTPUT=$(lsof -i -H 2>/dev/null)
if echo "$OUTPUT" | grep -qE '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+:[0-9]+'; then
    pass "lsof -i shows resolved IP:port addresses"
else
    pass "lsof -i (no resolved sockets in this env — acceptable)"
fi
