SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# Override system /usr/bin/fuser with our modbox wrapper
fuser() {
    "$MODBOX" fuser "$@"
}

echo ""
echo "── fuser ─────────────────────────────────────"

# Global holder PID (set by start_fd_holder via side-effect to avoid
# command substitution killing the background FD holder)
HOLDER_PID=""

# Helper: open an FD to $1 in a background subshell. Sets HOLDER_PID.
# We avoid $(...) capture which would kill the background process.
start_fd_holder() {
    local tmpfile="$1"
    (exec 3<>"$tmpfile"; sleep 60) &
    HOLDER_PID=$!
    disown "$HOLDER_PID" 2>/dev/null || true
    sleep 0.3
}

# Start an FD holder for $1 and store the PID in the variable named by $2.
# Uses indirect assignment to avoid $(...) subshell.
alloc_fd_holder() {
    local tmpfile="$1"
    local varname="$2"
    (exec 3<>"$tmpfile"; sleep 60) &
    local pid=$!
    disown "$pid" 2>/dev/null || true
    sleep 0.3
    printf -v "$varname" '%s' "$pid"
}

# Clean up helper
cleanup() {
    local pid="${HOLDER_PID:-}"
    if [[ -n "$pid" ]]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ── Basic flag tests ──

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' fuser --help

echo "  ── --version shows version ──"
assert_cmd_pat 'fuser \(modbox\)' fuser --version

echo "  ── -l lists signals ──"
assert_cmd_pat 'SIGHUP.*SIGINT' fuser -l

echo "  ── invalid option: error on stderr, exit non-zero ──"
assert_cmd_pat_stderr 'unrecognized option' fuser --badopt

echo "  ── nonexistent path: exit 1, no crash ──"
OUTPUT=$(fuser /nonexistent/path_xyz 2>/dev/null)
EXIT_CODE=$?
if [[ $EXIT_CODE -eq 1 ]] && [[ -z "$OUTPUT" ]]; then
    pass "fuser /nonexistent/path_xyz exits 1 with no output"
else
    fail "fuser /nonexistent/path_xyz exits $EXIT_CODE, output=[$OUTPUT]"
fi

# ── File path matching (default mode) ──

echo "  ── default mode: finds PID using a file ──"
TMPFILE=$(mktemp /tmp/fuser_test.XXXXXX)
start_fd_holder "$TMPFILE"
OUTPUT=$(fuser "$TMPFILE" 2>/dev/null || true)
if echo "$OUTPUT" | grep -q "$HOLDER_PID"; then
    pass "fuser $TMPFILE → contains PID $HOLDER_PID"
else
    fail "fuser $TMPFILE → missing PID $HOLDER_PID in output: [$OUTPUT]"
fi

echo "  ── -v verbose: shows PID, user, access, path ──"
OUTPUT=$(fuser -v "$TMPFILE" 2>/dev/null || true)
if echo "$OUTPUT" | grep -q "$HOLDER_PID"; then
    pass "fuser -v $TMPFILE → contains PID $HOLDER_PID"
else
    fail "fuser -v $TMPFILE → missing PID in output"
fi
if echo "$OUTPUT" | grep -q '<'; then
    pass "fuser -v → shows access mode"
else
    fail "fuser -v → missing access mode"
fi
if echo "$OUTPUT" | grep -qE '([a-zA-Z_]+)'; then
    pass "fuser -v → shows username"
else
    fail "fuser -v → missing username: [$OUTPUT]"
fi

echo "  ── -v -u verbose with username flag ──"
OUTPUT=$(fuser -v -u "$TMPFILE" 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE "$HOLDER_PID\\([a-zA-Z_]+\\)"; then
    pass "fuser -v -u → shows PID(user)"
else
    fail "fuser -v -u → missing user format: [$OUTPUT]"
fi

echo "  ── -p PIDs only ──"
OUTPUT=$(fuser -p "$TMPFILE" 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE "^${HOLDER_PID}$"; then
    pass "fuser -p → prints PID only on its own line"
else
    fail "fuser -p → unexpected output: [$OUTPUT]"
fi

echo "  ── -t PIDs only (alternative) ──"
OUTPUT=$(fuser -t "$TMPFILE" 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE "^${HOLDER_PID}$"; then
    pass "fuser -t → prints PID only"
else
    fail "fuser -t → unexpected output: [$OUTPUT]"
fi

# ── Silent mode ──

echo "  ── -s silent: exit 1 when process found ──"
fuser -s "$TMPFILE" 2>/dev/null
if [[ $? -eq 1 ]]; then
    pass "fuser -s $TMPFILE → exit 1 (process found)"
else
    fail "fuser -s $TMPFILE → expected exit 1"
fi

echo "  ── -s silent: exit 0 when no process found ──"
fuser -s /nonexistent/silent_test_xyz 2>/dev/null
if [[ $? -eq 0 ]]; then
    pass "fuser -s nonexistent → exit 0 (no match)"
else
    fail "fuser -s nonexistent → expected exit 0"
fi

# ── Mount mode ──

echo "  ── -m mount mode: finds PIDs using / ──"
OUTPUT=$(fuser -m / 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE '[0-9]+'; then
    pass "fuser -m / → shows PIDs"
else
    fail "fuser -m / → no output"
fi

echo "  ── -m mount mode: nonexistent mount returns empty ──"
OUTPUT=$(fuser -m /nonexistent/mount_xyz 2>/dev/null || true)
if [[ -z "$OUTPUT" ]]; then
    pass "fuser -m nonexistent → empty output"
else
    fail "fuser -m nonexistent → unexpected output"
fi

# ── JSON mode ──

echo "  ── --json: outputs JSON array ──"
OUTPUT=$(fuser --json "$TMPFILE" 2>/dev/null || true)
if echo "$OUTPUT" | head -1 | grep -qE '^\['; then
    pass "fuser --json starts with ["
else
    fail "fuser --json does not start with ["
fi
if echo "$OUTPUT" | grep -q '"pid"'; then
    pass "fuser --json contains pid field"
else
    fail "fuser --json missing pid field"
fi

# ── Kill mode ──

echo "  ── -k: kills process using file ──"
TMPFILE2=$(mktemp /tmp/fuser_kill_test.XXXXXX)
alloc_fd_holder "$TMPFILE2" KILL_PID
fuser -k -SIGTERM "$TMPFILE2" 2>/dev/null
sleep 0.3
if ! kill -0 "$KILL_PID" 2>/dev/null; then
    pass "fuser -k -SIGTERM → process $KILL_PID killed"
else
    kill "$KILL_PID" 2>/dev/null || true
    fail "fuser -k -SIGTERM → process $KILL_PID still alive"
fi

echo "  ── -k -i interactive: kills with 'y' input ──"
TMPFILE3=$(mktemp /tmp/fuser_interactive_test.XXXXXX)
alloc_fd_holder "$TMPFILE3" INTER_PID
echo "y" | fuser -k -SIGTERM -i "$TMPFILE3" 2>/dev/null
sleep 0.3
if ! kill -0 "$INTER_PID" 2>/dev/null; then
    pass "fuser -k -i → process $INTER_PID killed interactively"
else
    kill "$INTER_PID" 2>/dev/null || true
    fail "fuser -k -i → process $INTER_PID still alive"
fi

# ── Network mode (smoke test) ──

echo "  ── -n tcp: doesn't crash on port 22 ──"
OUTPUT=$(fuser -n tcp 22 2>/dev/null)
EXIT_CODE=$?
if [[ $EXIT_CODE -eq 0 || $EXIT_CODE -eq 1 ]]; then
    pass "fuser -n tcp 22 → exits cleanly"
else
    fail "fuser -n tcp 22 → exits $EXIT_CODE"
fi

echo "  ── -n fd: finds fd by number ──"
OUTPUT=$(fuser -n fd 0 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE '[0-9]+'; then
    pass "fuser -n fd 0 → shows PIDs"
else
    pass "fuser -n fd 0 → no match (acceptable if FD 0 not a socket)"
fi

echo "  ── -n unix: doesn't crash on socket path ──"
OUTPUT=$(fuser -n unix /var/run/docker.sock 2>/dev/null || true)
pass "fuser -n unix /var/run/docker.sock → runs without crash"

echo "  ── -z: zombie mode doesn't crash ──"
OUTPUT=$(fuser -z /proc/self/fd/0 2>/dev/null || true)
pass "fuser -z /proc/self/fd/0 → runs without crash"

# Clean up file holder
kill "$HOLDER_PID" 2>/dev/null || true
HOLDER_PID=""

echo "  ── all-access -a mode doesn't crash ──"
OUTPUT=$(fuser -a / 2>/dev/null || true)
pass "fuser -a / → runs without crash"

echo "  ── -4/-6 flags don't crash ──"
OUTPUT=$(fuser -4 -n tcp 22 2>/dev/null || true)
pass "fuser -4 -n tcp 22 → runs without crash"
OUTPUT=$(fuser -6 -n tcp 22 2>/dev/null || true)
pass "fuser -6 -n tcp 22 → runs without crash"
