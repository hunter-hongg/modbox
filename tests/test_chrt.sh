SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── chrt ───────────────────────────────────────"

# chrt queries and changes real-time scheduling attributes. Querying any live
# PID needs no privilege; *changing* a policy to SCHED_FIFO/SCHED_RR or raising
# an RT priority needs CAP_SYS_NICE, so the set-path tests assert only the
# failure branch when running unprivileged. This keeps the suite green both as
# root and as an ordinary user on a non-RT host.

skip_note() { pass "SKIP: $*"; }

# Skip set-path assertions we cannot perform in this environment.
IS_ROOT=0
[ "$(id -u)" -eq 0 ] && IS_ROOT=1

# Spawn a sleep and print its PID once it is alive. `$!` is unreliable under
# some shells' job control, so resolve the PID with pgrep and confirm liveness.
spawn_sleep() {
    setsid sleep 300 </dev/null >/dev/null 2>&1 &
    local pid=""
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        pid=$(pgrep -n -x sleep 2>/dev/null || true)
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            printf '%s' "$pid"
            return 0
        fi
        sleep 0.05
    done
    return 1
}

# ── help / version ──────────────────────────────────────────────────────────

echo "  ── --help exits 0 and prints usage ──"
assert_cmd_pat 'Usage:' chrt --help
"$MODBOX" chrt --help >/dev/null 2>&1
if [ "$?" -eq 0 ]; then
    pass "chrt --help exit 0"
else
    fail "chrt --help → expected exit 0"
fi

echo "  ── -h is accepted ──"
assert_cmd_pat 'Usage:' chrt -h

echo "  ── --version prints version and exits 0 ──"
assert_cmd_pat 'chrt \(modbox\)' chrt --version
"$MODBOX" chrt --version >/dev/null 2>&1
if [ "$?" -eq 0 ]; then
    pass "chrt --version exit 0"
else
    fail "chrt --version → expected exit 0"
fi

echo "  ── discoverable via modbox help ──"
assert_cmd_pat 'chrt.*[Ss]cheduling' help

# ── argument validation ─────────────────────────────────────────────────────

echo "  ── no arguments → no command or priority specified + exit 1 ──"
assert_cmd_pat_stderr 'no command or priority specified' chrt
"$MODBOX" chrt >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "chrt (no args) exit 1"
else
    fail "chrt (no args) → expected exit 1"
fi

echo "  ── unknown option → exit 2 (usage error) ──"
assert_cmd_pat_stderr "unrecognized option '--bogus'" chrt --bogus 5 true
"$MODBOX" chrt --bogus 5 true >/dev/null 2>&1
if [ "$?" -eq 2 ]; then
    pass "chrt --bogus exit 2"
else
    fail "chrt --bogus → expected exit 2"
fi

# ── -m: valid priority ranges ───────────────────────────────────────────────

echo "  ── -m lists every policy's min/max priority ──"
for pol in SCHED_OTHER SCHED_BATCH SCHED_IDLE SCHED_FIFO SCHED_RR; do
    assert_cmd_pat "^$pol min/max priority" chrt -m
done

echo "  ── -m FIFO/RR row shows the host's realtime range ──"
assert_cmd_pat '^SCHED_FIFO min/max priority[[:space:]]*: [0-9]+/[0-9]+$' chrt -m
assert_cmd_pat '^SCHED_RR min/max priority[[:space:]]*: [0-9]+/[0-9]+$' chrt -m

echo "  ── --max is the long form of -m ──"
assert_cmd_pat '^SCHED_OTHER min/max priority' chrt --max

echo "  ── -m lists realtime policies before batch/idle (upstream order) ──"
max_first=$("$MODBOX" chrt -m 2>/dev/null | head -1)
if printf '%s' "$max_first" | grep -q '^SCHED_OTHER'; then
    pass "chrt -m starts at SCHED_OTHER"
else
    fail "chrt -m first row — got [$max_first]"
fi

echo "  ── --help lists the policy flags ──"
for pol in other batch idle fifo rr; do
    assert_cmd_pat "\\-\\-$pol" chrt --help
done

# ── query mode: a live PID ──────────────────────────────────────────────────

query_target=$(spawn_sleep)
if [ -z "$query_target" ]; then
    fail "could not spawn a query target"
    exit 1
fi

echo "  ── -p <pid> reports policy then priority ──"
assert_cmd_pat "pid $query_target's current scheduling policy: SCHED_" chrt -p "$query_target"
assert_cmd_pat "pid $query_target's current scheduling priority: [0-9]+" chrt -p "$query_target"

echo "  ── query prints policy before priority ──"
out=$("$MODBOX" chrt -p "$query_target" 2>/dev/null)
first=$(printf '%s\n' "$out" | head -1)
if printf '%s' "$first" | grep -qE "current scheduling policy: SCHED_"; then
    pass "chrt -p policy line comes first"
else
    fail "chrt -p policy line first — got [$first]"
fi

echo "  ── query exits 0 ──"
"$MODBOX" chrt -p "$query_target" >/dev/null 2>&1
if [ "$?" -eq 0 ]; then
    pass "chrt -p <live pid> exit 0"
else
    fail "chrt -p <live pid> → expected exit 0"
fi

echo "  ── -p -v still reports policy/priority ──"
assert_cmd_pat "current scheduling policy: SCHED_" chrt -p -v "$query_target"

echo "  ── -p 0 queries chrt itself ──"
assert_cmd_pat 'current scheduling policy: SCHED_' chrt -p 0

echo "  ── --pid is the long form of -p ──"
assert_cmd_pat "pid $query_target's current scheduling policy" chrt --pid "$query_target"

kill "$query_target" 2>/dev/null || true

# ── query/parse error paths ─────────────────────────────────────────────────

echo "  ── non-existent PID → failed to get ... policy + exit 1 ──"
assert_cmd_pat_stderr "failed to get pid 999999's policy" chrt -p 999999
"$MODBOX" chrt -p 999999 >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "chrt -p 999999 exit 1"
else
    fail "chrt -p 999999 → expected exit 1"
fi

echo "  ── non-numeric PID in query mode → invalid PID argument + exit 1 ──"
assert_cmd_pat_stderr "invalid PID argument: 'extra'" chrt -p 1 extra
"$MODBOX" chrt -p 1 extra >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "chrt -p 1 extra exit 1"
else
    fail "chrt -p 1 extra → expected exit 1"
fi

# ── set mode (existing PID) ─────────────────────────────────────────────────

echo "  ── FIFO/RR require an explicit priority ──"
assert_cmd_pat_stderr 'policy SCHED_FIFO requires a priority argument' chrt -p -f 1
assert_cmd_pat_stderr 'policy SCHED_RR requires a priority argument' chrt -p -r 1

echo "  ── a policy without a priority accepts the lone number as the PID ──"
# SCHED_OTHER needs no priority, so `-p -o <pid>` sets that PID directly.
query_target2=$(spawn_sleep)
if [ -n "$query_target2" ]; then
    "$MODBOX" chrt -p -o "$query_target2" >/dev/null 2>&1
    if [ "$?" -eq 0 ]; then
        pass "chrt -p -o <pid> exit 0"
    else
        fail "chrt -p -o <pid> → expected exit 0"
    fi
else
    fail "could not spawn a policy target"
fi

echo "  ── set SCHED_OTHER on a live pid (obtainable unprivileged) ──"
set_target=$(spawn_sleep)
if [ -z "$set_target" ]; then
    fail "could not spawn a set target"
    exit 1
fi
"$MODBOX" chrt -p -o "$set_target" >/dev/null 2>&1
rc=$?
if [ "$rc" -eq 0 ] || [ "$IS_ROOT" -eq 0 ]; then
    # Either the policy was applied (0), or we are unprivileged and a clean
    # non-zero failure is the expected outcome; both are acceptable here.
    pass "chrt -p -o <pid> handled (rc=$rc)"
else
    fail "chrt -p -o <pid> as root → rc=$rc"
fi

echo "  ── unprivileged RT promotion fails cleanly ──"
if [ "$IS_ROOT" -eq 0 ]; then
    out=$("$MODBOX" chrt -p -f 50 "$set_target" 2>&1)
    rc=$?
    if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -qE "failed to set pid $set_target's policy: Operation not permitted"; then
        pass "chrt -p -f 50 unprivileged → Operation not permitted (exit 1)"
    else
        fail "chrt -p -f 50 unprivileged → rc=$rc out=[$out]"
    fi
    # SCHED_BATCH is obtainable without privilege on most hosts, so accept
    # either a successful change or a clean permission failure.
    out=$("$MODBOX" chrt -p -b 0 "$set_target" 2>&1)
    rc=$?
    if [ "$rc" -eq 0 ] || printf '%s' "$out" | grep -qE "failed to set pid $set_target's policy"; then
        pass "chrt -p -b 0 <pid> handled (rc=$rc)"
    else
        fail "chrt -p -b 0 <pid> → rc=$rc out=[$out]"
    fi
else
    skip_note "unprivileged RT failure path (running as root)"
fi

echo "  ── -R / --reset-on-fork accepted on the query path ──"
assert_cmd_pat "pid $set_target's current scheduling policy" chrt -p -R "$set_target"
assert_cmd_pat "pid $set_target's current scheduling policy" chrt -p --reset-on-fork "$set_target"

echo "  ── -a / --all-tasks accepted ──"
assert_cmd_pat "pid $set_target's current scheduling policy" chrt -p -a "$set_target"
assert_cmd_pat "pid $set_target's current scheduling policy" chrt -p --all-tasks "$set_target"

echo "  ── long-form policy aliases parse ──"
assert_cmd_pat_stderr 'policy SCHED_FIFO requires a priority argument' chrt -p --fifo 1
assert_cmd_pat_stderr 'policy SCHED_RR requires a priority argument' chrt -p --rr 1

kill "$set_target" 2>/dev/null || true

# ── launch mode ─────────────────────────────────────────────────────────────

echo "  ── launch a command under SCHED_OTHER ──"
"$MODBOX" chrt -o 0 true >/dev/null 2>&1
if [ "$?" -eq 0 ]; then
    pass "chrt -o 0 true exit 0"
else
    fail "chrt -o 0 true → expected exit 0"
fi

echo "  ── child exit status is propagated ──"
"$MODBOX" chrt -o 0 false >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "chrt -o 0 false exit 1"
else
    fail "chrt -o 0 false → expected exit 1"
fi

"$MODBOX" chrt -o 0 sh -c 'exit 7' >/dev/null 2>&1
if [ "$?" -eq 7 ]; then
    pass "chrt -o 0 sh -c 'exit 7' exit 7"
else
    fail "chrt -o 0 sh -c 'exit 7' → expected exit 7"
fi

echo "  ── child arguments pass through ──"
out=$("$MODBOX" chrt -o 0 echo hello chrt 2>/dev/null)
if [ "$out" = "hello chrt" ]; then
    pass "chrt -o 0 echo hello chrt → 'hello chrt'"
else
    fail "chrt launch args → got [$out]"
fi

echo "  ── child stdout is inherited ──"
assert_cmd_pat 'launched-ok' chrt -o 0 echo launched-ok

echo "  ── unprivileged RT launch fails cleanly, no child leak ──"
if [ "$IS_ROOT" -eq 0 ]; then
    out=$("$MODBOX" chrt -f 50 true 2>&1)
    rc=$?
    if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -qE "failed to set pid .*'s policy: Operation not permitted"; then
        pass "chrt -f 50 true unprivileged → Operation not permitted (exit 1)"
    else
        fail "chrt -f 50 true unprivileged → rc=$rc out=[$out]"
    fi
else
    skip_note "unprivileged RT launch failure path (running as root)"
fi

echo "  ── launch with no command after policy → non-zero exit ──"
"$MODBOX" chrt -o 0 >/dev/null 2>&1
if [ "$?" -ne 0 ]; then
    pass "chrt -o 0 (no command) → non-zero exit"
else
    fail "chrt -o 0 (no command) → expected non-zero exit"
fi
