SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── rmdir ────────────────────────────────────"

echo " ── basic remove ──"
mkdir -p "$TMPDIR"/rmdir_basic
assert_cmd "" rmdir "$TMPDIR"/rmdir_basic
test -d "$TMPDIR"/rmdir_basic && fail "rmdir_basic still exists" || pass "rmdir removed directory"

echo " ── non-empty directory fails ──"
mkdir -p "$TMPDIR"/rmdir_nonempty/child
assert_cmd_pat_stderr "not empty" rmdir "$TMPDIR"/rmdir_nonempty

echo " ── -p parents recursive ──"
mkdir -p "$TMPDIR"/rmdir_p/a/b/c
2>/dev/null rmdir -p "$TMPDIR"/rmdir_p/a/b/c
test -d "$TMPDIR"/rmdir_p && fail "rmdir_p still exists (ancestor not removed)" || pass "rmdir -p removed nested dirs and empty ancestors"

echo " ── -p stops at non-empty ancestor ──"
mkdir -p "$TMPDIR"/rmdir_p2/keep_me/a/b
touch "$TMPDIR"/rmdir_p2/keep_me/keep_this
2>/dev/null rmdir -p "$TMPDIR"/rmdir_p2/keep_me/a/b
test -f "$TMPDIR"/rmdir_p2/keep_me/keep_this && pass "rmdir -p stopped at non-empty ancestor" || fail "rmdir -p removed too much"

echo " ── help ──"
assert_cmd_pat 'Usage:' rmdir --help
