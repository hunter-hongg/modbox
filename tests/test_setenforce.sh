SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── setenforce ────────────────────────────────────"

echo "  ── no argument → usage error ──"
assert_cmd_pat_stderr 'usage:' setenforce

echo "  ── invalid numeric argument ──"
assert_cmd_pat_stderr 'usage:' setenforce 2

echo "  ── invalid string argument ──"
assert_cmd_pat_stderr 'usage:' setenforce foo

echo "  ── too many arguments ──"
assert_cmd_pat_stderr 'unexpected argument' setenforce 1 0

echo "  ── --help ──"
assert_cmd_pat 'Usage:' setenforce --help

echo "  ── --version ──"
assert_cmd_pat 'setenforce \(modbox\) 1\.0' setenforce --version

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'unrecognized option' setenforce --foo

echo "  ── stdout clean (no extra output) ──"
output=$("$MODBOX" setenforce 2>/dev/null || true)
if [[ -z "$output" ]]; then
    pass "setenforce produces no stdout output"
else
    fail "setenforce — expected empty stdout, got [$output]"
fi

echo "  ── SELinux disabled error (conditional) ──"
if [[ -f /sys/fs/selinux/enforce ]]; then
    echo "  SKIP — SELinux is enabled on this system"
else
    assert_cmd_pat_stderr 'SELinux is disabled' setenforce 1
fi

echo "  ── conditional mode change (root + SELinux only) ──"
if [[ -f /sys/fs/selinux/enforce ]] && [[ "$(id -u)" -eq 0 ]]; then
    original=$("$MODBOX" getenforce 2>/dev/null)
    if [[ "$original" == "Enforcing" ]]; then
        "$MODBOX" setenforce 0 2>/dev/null
        result=$("$MODBOX" getenforce 2>/dev/null)
        "$MODBOX" setenforce 1 2>/dev/null
        if [[ "$result" == "Permissive" ]]; then
            pass "setenforce 0 switches to Permissive"
        else
            fail "setenforce 0 — expected Permissive, got [$result]"
        fi
    else
        "$MODBOX" setenforce 1 2>/dev/null
        result=$("$MODBOX" getenforce 2>/dev/null)
        "$MODBOX" setenforce 0 2>/dev/null
        if [[ "$result" == "Enforcing" ]]; then
            pass "setenforce 1 switches to Enforcing"
        else
            fail "setenforce 1 — expected Enforcing, got [$result]"
        fi
    fi
elif [[ -f /sys/fs/selinux/enforce ]]; then
    echo "  SKIP — not running as root"
else
    echo "  SKIP — SELinux not available on this system"
fi

echo "  ── Enforcing/Permissive mnemonics (case-insensitive) ──"
if [[ -f /sys/fs/selinux/enforce ]] && [[ "$(id -u)" -eq 0 ]]; then
    original=$("$MODBOX" getenforce 2>/dev/null)
    if [[ "$original" == "Enforcing" ]]; then
        "$MODBOX" setenforce permissive 2>/dev/null
        result=$("$MODBOX" getenforce 2>/dev/null)
        "$MODBOX" setenforce enforcing 2>/dev/null
        if [[ "$result" == "Permissive" ]]; then
            pass "setenforce permissive (lowercase) works"
        else
            fail "setenforce permissive — expected Permissive, got [$result]"
        fi
    else
        "$MODBOX" setenforce ENFORCING 2>/dev/null
        result=$("$MODBOX" getenforce 2>/dev/null)
        "$MODBOX" setenforce Permissive 2>/dev/null
        if [[ "$result" == "Enforcing" ]]; then
            pass "setenforce ENFORCING (uppercase) works"
        else
            fail "setenforce ENFORCING — expected Enforcing, got [$result]"
        fi
    fi
else
    echo "  SKIP — requires root + SELinux"
fi
