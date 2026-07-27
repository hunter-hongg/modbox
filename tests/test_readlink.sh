SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── readlink ────────────────────────────────────"

echo "  ── basic symlink reading ──"
echo "hello" > "$TMPDIR/rl_target"
ln -sf "$TMPDIR/rl_target" "$TMPDIR/rl_link"
assert_cmd "$TMPDIR/rl_target" readlink "$TMPDIR/rl_link"

echo "  ── -f canonicalization ──"
assert_cmd "$TMPDIR/rl_target" readlink -f "$TMPDIR/rl_link"

echo "  ── non-symlink error ──"
assert_cmd_pat_stderr 'not a symbolic link' readlink "$TMPDIR/rl_target"

echo "  ── -q quiet on non-symlink ──"
result=$("$MODBOX" readlink -q "$TMPDIR/rl_target" 2>&1)
if [[ -z "$result" ]]; then
    pass "readlink -q non-symlink produces no output"
else
    fail "readlink -q — expected no output, got [$result]"
fi

echo "  ── --help ──"
assert_cmd_pat 'canonicalize' readlink --help

echo "  ── --version ──"
assert_cmd_pat 'readlink \(modbox\) 1\.0' readlink -V
