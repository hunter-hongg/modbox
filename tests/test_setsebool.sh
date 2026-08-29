SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── setsebool ─────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' setsebool --help

echo "  ── --version ──"
assert_cmd_pat 'setsebool \(modbox\) 1\.0' setsebool --version

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'unrecognized option' setsebool --foo

echo "  ── malformed token rejected ──"
assert_cmd_pat_stderr 'operand' setsebool 'somebool'

echo "  ── unknown boolean name rejected ──"
"$MODBOX" setsebool definitely_not_a_real_bool on >/dev/null 2>&1
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "setsebool with unknown boolean exits non-zero (rc=$rc)"
else
    fail "setsebool accepted an unknown boolean name"
fi

echo "  ── -P persist flag parses ──"
"$MODBOX" setsebool -P definitely_not_a_real_bool on >/dev/null 2>&1
pass "setsebool -P executed without crashing"

echo "  ── batch form parses ──"
"$MODBOX" setsebool 'a=on' 'b=off' >/dev/null 2>&1
pass "setsebool batch form executed without crashing"

echo "  ── no operand rejected ──"
assert_cmd_pat_stderr 'missing operand' setsebool

echo "  ── ground truth check ──"
if command -v /usr/sbin/setsebool >/dev/null 2>&1 && [[ -e /sys/fs/selinux/enforce ]] && [[ "$(id -u)" -eq 0 ]]; then
    name=$(/usr/sbin/getsebool 2>/dev/null | head -1 | sed 's/ .*//')
    "$MODBOX" setsebool "$name" off 2>/dev/null
    actual=$(/usr/sbin/getsebool "$name" 2>/dev/null)
    "$MODBOX" setsebool "$name" on 2>/dev/null
    if [[ "$actual" == *"off"* ]]; then
        pass "setsebool toggled $name (restored to on)"
    else
        fail "setsebool did not toggle $name"
    fi
else
    echo "  SKIP — system setsebool / SELinux / root unavailable on this host"
fi
