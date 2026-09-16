SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── stty ────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' stty --help
assert_cmd_pat 'Print or change terminal characteristics' stty --help
assert_cmd_pat '\-a, --all' stty --help
assert_cmd_pat '\-g, --save' stty --help
assert_cmd_pat 'DEVICE' stty --help

echo "  ── --version ──"
assert_cmd_pat 'stty \(modbox\)' stty --version

echo "  ── non-tty stdin is an error (GNU exit status 1) ──"
# Both GNU stty and modbox must reject a non-terminal stdin with -1, and must
# say so on stderr rather than exiting 0 silently.
"$MODBOX" stty </dev/null >/dev/null 2>"$TMPDIR/stty_err"; rc=$?
if [[ $rc -eq 1 ]]; then
    pass "stty on non-tty exits 1"
else
    fail "stty on non-tty — expected exit 1, got $rc"
fi
if grep -qE 'Inappropriate ioctl|not a tty' "$TMPDIR/stty_err"; then
    pass "stty on non-tty reports the failure on stderr"
else
    fail "stty on non-tty — expected an ioctl/tty error on stderr"
fi

"$MODBOX" stty -a </dev/null >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then
    pass "stty -a on non-tty exits 1"
else
    fail "stty -a on non-tty — expected exit 1, got $rc"
fi

"$MODBOX" stty size </dev/null >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then
    pass "stty size on non-tty exits 1"
else
    fail "stty size on non-tty — expected exit 1, got $rc"
fi

echo "  ── -F DEVICE: unreadable device is an error ──"
"$MODBOX" stty -F /nonexistent_stty_device_xyz </dev/null >/dev/null 2>"$TMPDIR/stty_err"; rc=$?
if [[ $rc -eq 1 ]]; then
    pass "stty -F <missing> exits 1"
else
    fail "stty -F <missing> — expected exit 1, got $rc"
fi
if grep -qE '/nonexistent_stty_device_xyz' "$TMPDIR/stty_err"; then
    pass "stty -F <missing> names the device in the error"
else
    fail "stty -F <missing> — expected the device name on stderr"
fi

echo "  ── -F DEVICE: non-tty device is an error and named ──"
# A character device that exists but is not a terminal: GNU names the device
# it was asked to use, not "standard input".
"$MODBOX" stty -F /dev/null </dev/null >/dev/null 2>"$TMPDIR/stty_err"; rc=$?
if [[ $rc -eq 1 ]]; then
    pass "stty -F /dev/null exits 1"
else
    fail "stty -F /dev/null — expected exit 1, got $rc"
fi
if grep -qE '/dev/null' "$TMPDIR/stty_err"; then
    pass "stty -F /dev/null names the device, not 'standard input'"
else
    fail "stty -F /dev/null — expected '/dev/null' on stderr, got: $(cat "$TMPDIR/stty_err")"
fi
if grep -qE 'standard input' "$TMPDIR/stty_err"; then
    fail "stty -F /dev/null — must not claim 'standard input' when -F was given"
else
    pass "stty -F /dev/null does not fall back to 'standard input'"
fi

echo "  ── interactive behaviour behind a pseudo-terminal ──"
if command -v script >/dev/null 2>&1; then
    # `script` allocates a real PTY so the success paths can be exercised in CI.
    "$MODBOX" stty -a >/dev/null 2>&1
    pty_out=$(script -qec "'$MODBOX' stty -a" /dev/null 2>/dev/null | tr -d '\r')
    if printf '%s' "$pty_out" | grep -qE '^speed [0-9]+ baud;'; then
        pass "stty -a reports the line speed under a PTY"
    else
        fail "stty -a under a PTY — expected 'speed <n> baud;', got: $(printf '%s' "$pty_out" | head -c 80)"
    fi
    if printf '%s' "$pty_out" | grep -qE 'rows [0-9]+; columns [0-9]+'; then
        pass "stty -a reports rows/columns under a PTY"
    else
        fail "stty -a under a PTY — expected rows/columns"
    fi
    if printf '%s' "$pty_out" | grep -qE 'intr = \^C'; then
        pass "stty -a reports the intr control character"
    else
        fail "stty -a under a PTY — expected 'intr = ^C'"
    fi

    pty_size=$(script -qec "'$MODBOX' stty size" /dev/null 2>/dev/null | tr -d '\r')
    if printf '%s' "$pty_size" | grep -qE '^[0-9]+ [0-9]+$'; then
        pass "stty size prints '<rows> <cols>' under a PTY"
    else
        fail "stty size under a PTY — expected two integers, got: $(printf '%s' "$pty_size" | head -c 40)"
    fi

    pty_speed=$(script -qec "'$MODBOX' stty speed" /dev/null 2>/dev/null | tr -d '\r')
    if printf '%s' "$pty_speed" | grep -qE '^[0-9]+$'; then
        pass "stty speed prints a bare baud rate under a PTY"
    else
        fail "stty speed under a PTY — expected a number, got: $(printf '%s' "$pty_speed" | head -c 40)"
    fi

    # -g must emit the stty-readable 6-field colon form, and feeding it straight
    # back must be accepted (round trip).
    pty_g=$(script -qec "'$MODBOX' stty -g" /dev/null 2>/dev/null | tr -d '\r' | head -1)
    if printf '%s' "$pty_g" | grep -qE '^[0-9a-f]+:[0-9a-f]+:[0-9a-f]+:[0-9a-f]+:[0-9a-f]+:[0-9a-f]+'; then
        pass "stty -g emits the stty-readable colon form"
    else
        fail "stty -g under a PTY — unexpected form: $(printf '%s' "$pty_g" | head -c 60)"
    fi
    if [[ -n "$pty_g" ]]; then
        script -qec "'$MODBOX' stty $pty_g" /dev/null >/dev/null 2>&1; rc=$?
        if [[ $rc -eq 0 ]]; then
            pass "stty -g output round-trips back into stty"
        else
            fail "stty -g round trip — expected exit 0, got $rc"
        fi
    fi

    # A setting that is not recognised must fail with a non-zero status.
    script -qec "'$MODBOX' stty bogus_setting_xyz >/dev/null 2>&1" /dev/null >/dev/null 2>&1
    rc=$?
    if [[ $rc -ne 0 ]]; then
        pass "stty rejects an unknown setting with a non-zero status"
    else
        fail "stty accepted the unknown setting 'bogus_setting_xyz'"
    fi

    # Negation form: reading back a known flag must work under both spellings.
    script -qec "'$MODBOX' stty echo" /dev/null >/dev/null 2>&1; rc=$?
    if [[ $rc -eq 0 ]]; then
        pass "stty accepts the 'echo' setting"
    else
        fail "stty echo — expected exit 0, got $rc"
    fi
    script -qec "'$MODBOX' stty -echo" /dev/null >/dev/null 2>&1; rc=$?
    if [[ $rc -eq 0 ]]; then
        pass "stty accepts the negated '-echo' setting"
    else
        fail "stty -echo — expected exit 0, got $rc"
    fi
else
    pass "skipped PTY assertions (script(1) not available)"
fi
