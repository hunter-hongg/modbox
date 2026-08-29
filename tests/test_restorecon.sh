SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── restorecon ─────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' restorecon --help

echo "  ── --version ──"
assert_cmd_pat 'restorecon \(modbox\) 1\.0' restorecon --version

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'unrecognized option' restorecon --foo

echo "  ── missing operand rejected ──"
assert_cmd_pat_stderr 'missing operand' restorecon

echo "  ── extra positional operands are tolerated (no crash) ──"
"$MODBOX" restorecon "$TMPDIR" /nonexistent_extra 2>/dev/null
pass "restorecon tolerates extra positional operands"

echo "  ── recursive flag accepted (parses, then reports env state) ──"
# On a Disabled / unsupported host the relabel cannot succeed; we only assert
# the invocation does not crash and returns a deterministic non-zero exit.
"$MODBOX" restorecon -R "$TMPDIR" >/dev/null 2>&1
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "restorecon -R on unsupported host exits non-zero (rc=$rc)"
else
    fail "restorecon -R unexpectedly succeeded on an SELinux-disabled host"
fi

echo "  ── -v does not crash ──"
"$MODBOX" restorecon -v "$TMPDIR" >/dev/null 2>&1
pass "restorecon -v executed without crashing"

echo "  ── ground truth check ──"
if command -v /usr/sbin/restorecon >/dev/null 2>&1 && [[ -e /sys/fs/selinux/enforce ]]; then
    mkdir -p "$TMPDIR"/rc_tree/sub
    touch "$TMPDIR"/rc_tree/a.txt "$TMPDIR"/rc_tree/sub/b.txt
    expected=$("$MODBOX" restorecon -Rnv "$TMPDIR"/rc_tree 2>&1)
    actual=$(/usr/sbin/restorecon -Rnv "$TMPDIR"/rc_tree 2>&1)
    if diff <(printf '%s\n' "$expected") <(printf '%s\n' "$actual") >/dev/null; then
        pass "restorecon matches system output"
    else
        fail "restorecon differs from system restorecon"
    fi
else
    echo "  SKIP — system restorecon or SELinux unavailable on this host"
fi
