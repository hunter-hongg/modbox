SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── pr ─────────────────────────────────────"

TMPF="$TMPDIR/pr_input"
printf 'a\nb\nc\nd\n' > "$TMPF"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' pr --help

echo "  ── plain file prints contents with header ──"
assert_cmd_pat 'Page 1' pr "$TMPF"
assert_cmd_pat '^a$' pr "$TMPF"

echo "  ── -t suppresses header ──"
assert_cmd_not_pat 'Page 1' pr -t "$TMPF"
assert_cmd_pat '^a' pr -t "$TMPF"

echo "  ── reads stdin ──"
out=$(printf 'x\ny\n' | "$MODBOX" pr -t)
if [[ "$out" == $'x\ny' ]]; then
    pass "stdin passthrough"
else
    fail "stdin passthrough got [$out]"
fi

echo "  ── -2 two columns ──"
assert_cmd_pat 'a' pr -2 -t "$TMPF"
assert_cmd_pat 'c' pr -2 -t "$TMPF"

echo "  ── --columns=2 two columns ──"
assert_cmd_pat 'c' pr --columns=2 -t "$TMPF"

echo "  ── -d double space ──"
out=$(printf 'a\nb\n' | "$MODBOX" pr -d -t)
lines=$(printf '%s\n' "$out" | wc -l)
if [[ "$lines" -ge 3 ]]; then
    pass "-d double spacing ($lines lines)"
else
    fail "-d double spacing got $lines lines"
fi

echo "  ── missing file errors ──"
assert_cmd_pat_stderr 'No such file' pr /nonexistent/xyz
