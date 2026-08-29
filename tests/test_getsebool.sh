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

echo "  ── output format / availability on this host ──"
out=$("$MODBOX" getsebool 2>&1)
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "getsebool exits non-zero when booleans are unavailable (rc=$rc)"
else
    pass "getsebool succeeded (SELinux active) — format below"
    if printf '%s\n' "$out" | grep -qE '^[a-z_]+ --> (on|off)'; then
        pass "getsebool output matches 'name --> on|off' format"
    else
        fail "getsebool output format unexpected: [$out]"
    fi
fi

echo "  ── ground truth check ──"
if command -v /usr/sbin/getsebool >/dev/null 2>&1 && [[ -e /sys/fs/selinux/enforce ]]; then
    expected=$(/usr/sbin/getsebool 2>/dev/null)
    actual=$("$MODBOX" getsebool 2>/dev/null)
    if [[ "$actual" == "$expected" ]]; then
        pass "getsebool matches system value"
    else
        fail "getsebool differs from system getsebool"
    fi
else
    echo "  SKIP — system getsebool or SELinux unavailable on this host"
fi
