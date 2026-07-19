SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── wc ───────────────────────────────────────"

printf 'hello world\nfoo bar baz\n' > "$TMPDIR"/wc_test.txt

echo "  ── default (lines words bytes) ──"
assert_cmd "$(printf '       2       5      24 %s/wc_test.txt' "$TMPDIR")" wc "$TMPDIR"/wc_test.txt

echo "  ── -l (lines) ──"
assert_cmd "$(printf '       2 %s/wc_test.txt' "$TMPDIR")" wc -l "$TMPDIR"/wc_test.txt

echo "  ── -w (words) ──"
assert_cmd "$(printf '       5 %s/wc_test.txt' "$TMPDIR")" wc -w "$TMPDIR"/wc_test.txt

echo "  ── -c (bytes) ──"
assert_cmd "$(printf '      24 %s/wc_test.txt' "$TMPDIR")" wc -c "$TMPDIR"/wc_test.txt

echo "  ── combined -cwl ──"
assert_cmd "$(printf '       2       5      24 %s/wc_test.txt' "$TMPDIR")" wc -cwl "$TMPDIR"/wc_test.txt

echo "  ── -m (chars) same as -c here ──"
assert_cmd "$(printf '      24 %s/wc_test.txt' "$TMPDIR")" wc -m "$TMPDIR"/wc_test.txt

echo "  ── multiple files with total ──"
assert_cmd "$(printf '       2 %s/wc_test.txt\n       2 %s/wc_test.txt\n       4 total' "$TMPDIR" "$TMPDIR")" wc -l "$TMPDIR"/wc_test.txt "$TMPDIR"/wc_test.txt

echo "  ── stdin ──"
result=$(printf 'a b\nc d e\n' | "$MODBOX" wc 2>/dev/null || true)
if [[ "$result" == "$(printf '       2       5      10')" ]]; then
    pass "wc (stdin)"
else
    fail "wc (stdin) — expected [2 5 10] got [$result]"
fi

echo "  ── - (stdin dash) ──"
result=$(printf 'x y z\n' | "$MODBOX" wc -l - 2>/dev/null || true)
if [[ "$result" == "$(printf '       1 -')" ]]; then
    pass "wc - (stdin dash)"
else
    fail "wc - (stdin dash) — expected [1 -] got [$result]"
fi

echo "  ── empty file ──"
: > "$TMPDIR"/wc_empty.txt
assert_cmd "$(printf '       0       0       0 %s/wc_empty.txt' "$TMPDIR")" wc "$TMPDIR"/wc_empty.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' wc --help
