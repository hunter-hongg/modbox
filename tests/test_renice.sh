SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── renice ─────────────────────────────────────"

# Niceness is clamped by the kernel to [-20, 19] and an unprivileged process
# may only *raise* it. A freshly spawned shell normally starts at 0, but a
# session that previously lowered another process (or was itself reniced) may
# be pegged at 19, in which case nothing further can be changed without
# privilege. Probe once and skip the raise-based cases when that happens, so
# the suite is green in any environment.

# skip_note prints a visible (always-passing) note instead of silently
# dropping assertions.
skip_note() { pass "SKIP: $*"; }

# Read the niceness of a PID (empty when the process is gone).
child_nice() { ps -o ni= -p "$1" 2>/dev/null | tr -d ' '; }

# Probe: can an unprivileged process still raise a child's niceness here?
sleep 300 &
_rc_probe=$!
sleep 0.1
_PROBE_NICE=$(child_nice "$_rc_probe")
kill "$_rc_probe" 2>/dev/null || true
CAN_RAISE=1
if [ "$(id -u)" -ne 0 ] && [ "${_PROBE_NICE:-0}" -ge 19 ]; then
    CAN_RAISE=0
fi
echo "  (child baseline niceness: ${_PROBE_NICE:-?}; raise-tests: $([ "$CAN_RAISE" -eq 1 ] && echo enabled || echo skipped))"

# ── help / version / basic argument validation ──────────────────────────────

echo "  ── --help exits 0 and prints usage ──"
assert_cmd_pat 'Usage:' renice --help
"$MODBOX" renice --help >/dev/null 2>&1
if [ "$?" -eq 0 ]; then
    pass "renice --help exit 0"
else
    fail "renice --help → expected exit 0"
fi

echo "  ── -h is accepted ──"
assert_cmd_pat 'Usage:' renice -h

echo "  ── --version prints version and exits 0 ──"
assert_cmd_pat 'renice \(modbox\)' renice --version
"$MODBOX" renice --version >/dev/null 2>&1
if [ "$?" -eq 0 ]; then
    pass "renice --version exit 0"
else
    fail "renice --version → expected exit 0"
fi

echo "  ── discoverable via modbox help ──"
assert_cmd_pat 'renice.*Alter priority' help

echo "  ── no arguments → not enough arguments ──"
assert_cmd_pat_stderr 'not enough arguments' renice
"$MODBOX" renice >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "renice (no args) exit 1"
else
    fail "renice (no args) → expected exit 1"
fi

echo "  ── priority without identifiers → not enough arguments ──"
assert_cmd_pat_stderr 'not enough arguments' renice -n 5
"$MODBOX" renice 5 >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "renice 5 (no ids) exit 1"
else
    fail "renice 5 (no ids) → expected exit 1"
fi

echo "  ── malformed priority → invalid priority ──"
assert_cmd_pat_stderr "invalid priority 'abc'" renice abc -p 1
"$MODBOX" renice abc -p 1 >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "renice abc exit 1"
else
    fail "renice abc → expected exit 1"
fi

# ── absolute priority on a spawned child ────────────────────────────────────

if [ "$CAN_RAISE" -eq 1 ]; then
    echo "  ── absolute -n sets a child's niceness ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    target=$((base + 5)); [ "$target" -gt 19 ] && target=19
    out=$("$MODBOX" renice -n "$target" -p "$child" 2>&1)
    rc=$?
    ni=$(child_nice "$child")
    if [ "$rc" -eq 0 ] && [ "$ni" = "$target" ]; then
        pass "renice -n $target -p $child ($base → $ni)"
    else
        fail "renice -n $target -p $child → rc=$rc ni=[$ni] out=[$out]"
    fi
    if printf '%s' "$out" | grep -qE "^$child \(process ID\) old priority [0-9-]+, new priority $target$"; then
        pass "renice report line format"
    else
        fail "renice report line — got [$out]"
    fi
    kill "$child" 2>/dev/null || true

    echo "  ── bare priority form is absolute (like --priority) ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    target=$((base + 3)); [ "$target" -gt 19 ] && target=19
    out=$("$MODBOX" renice "$target" -p "$child" 2>&1)
    rc=$?
    ni=$(child_nice "$child")
    if [ "$rc" -eq 0 ] && [ "$ni" = "$target" ]; then
        pass "renice $target -p $child (ni=$ni)"
    else
        fail "renice $target -p $child → rc=$rc ni=[$ni] out=[$out]"
    fi
    kill "$child" 2>/dev/null || true

    echo "  ── --priority is the absolute long form ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    target=$((base + 2)); [ "$target" -gt 19 ] && target=19
    "$MODBOX" renice --priority "$target" -p "$child" >/dev/null 2>&1
    ni=$(child_nice "$child")
    if [ "$ni" = "$target" ]; then
        pass "renice --priority $target (ni=$ni)"
    else
        fail "renice --priority $target → expected $target got [$ni] (base=$base)"
    fi
    kill "$child" 2>/dev/null || true

    echo "  ── -p is the default selector ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    target=$((base + 1)); [ "$target" -gt 19 ] && target=19
    "$MODBOX" renice -n "$target" "$child" >/dev/null 2>&1
    ni=$(child_nice "$child")
    if [ "$ni" = "$target" ]; then
        pass "renice -n $target $child (default -p, ni=$ni)"
    else
        fail "renice -n $target $child (default -p) → expected $target got [$ni]"
    fi
    kill "$child" 2>/dev/null || true

    # ── relative priority ───────────────────────────────────────────────────

    echo "  ── --relative adds a delta (clamped to 19) ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    delta=4
    expected=$((base + delta)); [ "$expected" -gt 19 ] && expected=19
    out=$("$MODBOX" renice --relative "$delta" -p "$child" 2>&1)
    rc=$?
    ni=$(child_nice "$child")
    if [ "$rc" -eq 0 ] && [ "$ni" = "$expected" ]; then
        pass "renice --relative $delta ($base → $ni, expected $expected)"
    else
        fail "renice --relative $delta → rc=$rc ni=[$ni] expected=[$expected] out=[$out]"
    fi
    if printf '%s' "$out" | grep -qE "old priority $base, new priority $expected"; then
        pass "renice --relative report shows old and new (clamped)"
    else
        fail "renice --relative report — got [$out]"
    fi
    kill "$child" 2>/dev/null || true

    echo "  ── POSIXLY_CORRECT makes -n relative ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    delta=3
    expected=$((base + delta)); [ "$expected" -gt 19 ] && expected=19
    POSIXLY_CORRECT=1 "$MODBOX" renice -n "$delta" -p "$child" >/dev/null 2>&1
    ni=$(child_nice "$child")
    if [ "$ni" = "$expected" ]; then
        pass "POSIXLY_CORRECT -n $delta ($base → $ni, expected $expected)"
    else
        fail "POSIXLY_CORRECT -n $delta → expected $expected got [$ni] (base=$base)"
    fi
    kill "$child" 2>/dev/null || true

    # ── multiple identifiers ────────────────────────────────────────────────

    echo "  ── multiple identifiers are all adjusted ──"
    sleep 300 &
    a=$!
    sleep 300 &
    b=$!
    sleep 0.1
    out=$("$MODBOX" renice -n 19 -p "$a" "$b" 2>&1)
    rc=$?
    nia=$(child_nice "$a")
    nib=$(child_nice "$b")
    lines=$(printf '%s\n' "$out" | grep -c 'process ID')
    if [ "$rc" -eq 0 ] && [ "$nia" = "19" ] && [ "$nib" = "19" ] && [ "$lines" -eq 2 ]; then
        pass "renice -n 19 -p <a> <b> (both ni=19, 2 lines)"
    else
        fail "renice multiple → rc=$rc ni=[$nia/$nib] lines=$lines out=[$out]"
    fi
    kill "$a" "$b" 2>/dev/null || true
else
    skip_note "raise-based cases (session already at niceness $_PROBE_NICE; no privilege)"
fi

# ── error paths ─────────────────────────────────────────────────────────────

echo "  ── non-existent PID → failed to get priority + exit 1 ──"
assert_cmd_pat_stderr 'failed to get priority for 999999' renice -n 5 -p 999999
"$MODBOX" renice -n 5 -p 999999 >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "renice non-existent pid exit 1"
else
    fail "renice non-existent pid → expected exit 1"
fi

echo "  ── non-numeric pid → bad process ID value + exit 1 ──"
assert_cmd_pat_stderr 'bad process ID value' renice -n 5 -p notanumber
"$MODBOX" renice -n 5 -p notanumber >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "renice bad pid exit 1"
else
    fail "renice bad pid → expected exit 1"
fi

echo "  ── non-numeric pgrp → bad process group ID value ──"
assert_cmd_pat_stderr 'bad process group ID value' renice -n 5 -g notanumber

echo "  ── unknown user → unknown user + exit 1 ──"
assert_cmd_pat_stderr 'unknown user zzz_no_such_user' renice -n 5 -u zzz_no_such_user
"$MODBOX" renice -n 5 -u zzz_no_such_user >/dev/null 2>&1
if [ "$?" -eq 1 ]; then
    pass "renice unknown user exit 1"
else
    fail "renice unknown user → expected exit 1"
fi

echo "  ── --priority=N attached form is rejected (like util-linux) ──"
assert_cmd_pat_stderr "invalid priority '--priority=19'" renice --priority=19 -p 1

echo "  ── --priority stays absolute even with POSIXLY_CORRECT ──"
if [ "$CAN_RAISE" -eq 1 ]; then
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    # -n under POSIXLY_CORRECT is relative, but --priority must remain
    # absolute and land exactly on the requested value.
    target=$((base + 6)); [ "$target" -gt 19 ] && target=19
    POSIXLY_CORRECT=1 "$MODBOX" renice --priority "$target" -p "$child" >/dev/null 2>&1
    ni=$(child_nice "$child")
    if [ "$ni" = "$target" ]; then
        pass "POSIXLY_CORRECT --priority $target is absolute ($base → $ni)"
    else
        fail "POSIXLY_CORRECT --priority $target → expected $target got [$ni] (base=$base)"
    fi
    kill "$child" 2>/dev/null || true
fi

echo "  ── -- terminates options ──"
assert_cmd_pat_stderr 'bad process ID value' renice -n 5 -- -p

echo "  ── unknown option is rejected ──"
"$MODBOX" renice --definitely-not-an-option -p 1 >/dev/null 2>&1
if [ "$?" -ne 0 ]; then
    pass "renice unknown option → non-zero exit"
else
    fail "renice unknown option → expected non-zero exit"
fi

# ── user target (name resolves to a UID) ────────────────────────────────────

echo "  ── -u <own user> resolves the name to a numeric UID ──"
me=$(id -un)
myuid=$(id -u)
out=$("$MODBOX" renice -n 19 -u "$me" 2>&1)
rc=$?
if printf '%s' "$out" | grep -qE "$myuid \(user ID\)"; then
    pass "renice -u $me resolved to UID $myuid"
else
    fail "renice -u $me → expected UID $myuid (user ID) in [$out]"
fi
if [ "$(id -u)" -ne 0 ] && [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -qE 'failed to set priority for .*\(user ID\)'; then
    pass "renice -u $me unprivileged → permission error, exit 1"
fi

# ── privilege error path (only when unprivileged and raisable) ──────────────

if [ "$(id -u)" -ne 0 ] && [ "$CAN_RAISE" -eq 1 ]; then
    echo "  ── lowering niceness unprivileged → Permission denied ──"
    sleep 300 &
    child=$!
    sleep 0.1
    base=$(child_nice "$child")
    lower=$((base - 1))
    out=$("$MODBOX" renice -n "$lower" -p "$child" 2>&1)
    rc=$?
    if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -qE 'failed to set priority for .*Permission denied'; then
        pass "renice lowering unprivileged → Permission denied (exit 1)"
    else
        fail "renice lowering unprivileged → rc=$rc out=[$out]"
    fi
    kill "$child" 2>/dev/null || true
fi
