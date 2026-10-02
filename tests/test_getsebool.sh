SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── getsebool ─────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' getsebool --help

echo "  ── --version ──"
assert_cmd_pat 'getsebool \(modbox\) 1\.0' getsebool --version

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'unrecognized option' getsebool --foo

if [[ ! -e /sys/fs/selinux/enforce ]]; then
    echo "  SKIP — SELinux unavailable on this host (no /sys/fs/selinux/enforce)"
    exit 0
fi

echo "  ── no operands is a usage error ──"
assert_cmd_pat_stderr 'usage:.*-a or .*boolean' getsebool
out=$("$MODBOX" getsebool 2>/dev/null); rc=$?
if [[ $rc -eq 1 && -z "$out" ]]; then
    pass "getsebool with no operands exits 1 and prints nothing on stdout"
else
    fail "getsebool with no operands should exit 1 with empty stdout (got rc=$rc, out=[$out])"
fi

echo "  ── -a reports every boolean ──"
out=$("$MODBOX" getsebool -a 2>/dev/null); rc=$?
if [[ $rc -eq 0 ]]; then
    pass "getsebool -a exits 0"
else
    fail "getsebool -a should exit 0 (got rc=$rc)"
fi
if printf '%s\n' "$out" | grep -qE '^[a-z_]+ --> (on|off)$'; then
    pass "getsebool -a output matches 'name --> on|off'"
else
    fail "getsebool -a output format unexpected: [$(printf '%s\n' "$out" | head -3 | tr '\n' '~')]"
fi

echo "  ── -a cannot be combined with boolean names ──"
assert_cmd_pat_stderr 'usage:.*-a or .*boolean' getsebool -a fips_mode

echo "  ── named booleans ──"
assert_cmd_pat '^fips_mode --> (on|off)$' getsebool fips_mode

echo "  ── an unknown boolean reports the error and exits 255 ──"
assert_cmd_pat_stderr 'Error getting active value for no_such_boolean_xyz' getsebool no_such_boolean_xyz
"$MODBOX" getsebool no_such_boolean_xyz >/dev/null 2>&1
rc=$?
if [[ $rc -eq 255 ]]; then
    pass "getsebool unknown boolean exits 255 (rc=$rc)"
else
    fail "getsebool unknown boolean should exit 255 (got rc=$rc)"
fi

echo "  ── processing stops at the first unknown boolean ──"
out=$("$MODBOX" getsebool no_such_boolean_xyz fips_mode 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "getsebool stops at the first failure (no trailing output)"
else
    fail "getsebool should print nothing after a failed boolean (got [$out])"
fi

echo "  ── ground truth check against the system getsebool ──"
if command -v getsebool >/dev/null 2>&1; then
    expected=$(/usr/sbin/getsebool -a 2>/dev/null)
    actual=$("$MODBOX" getsebool -a 2>/dev/null)
    if [[ "$actual" == "$expected" ]]; then
        pass "getsebool -a matches the system getsebool"
    else
        fail "getsebool -a differs from the system getsebool"
    fi
else
    echo "  SKIP — system getsebool unavailable on this host"
fi
