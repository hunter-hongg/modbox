echo ""
echo "── sort ──────────────────────────────────────"

printf 'banana\napple\ncherry\ndate\n' > "$TMPDIR"/sort_alpha
echo "  ── basic sort ──"
assert_cmd "$(printf 'apple\nbanana\ncherry\ndate\n')" sort "$TMPDIR"/sort_alpha

echo "  ── reverse sort ──"
assert_cmd "$(printf 'date\ncherry\nbanana\napple\n')" sort -r "$TMPDIR"/sort_alpha

echo "  ── numeric sort ──"
printf '10\n2\n30\n1\n20\n' > "$TMPDIR"/sort_num
assert_cmd "$(printf '1\n2\n10\n20\n30\n')" sort -n "$TMPDIR"/sort_num

echo "  ── ignore case ──"
printf 'Alpha\nBRAVO\ncharlie\nalpha\n' > "$TMPDIR"/sort_case
# After -f: Alpha/alpha equal, BRAVO equal to itself, charlie > alpha
# Output: Alpha, alpha, BRAVO, charlie (Alpha/alpha tiebroken by strcmp)
assert_cmd_pat '^Alpha$' sort -f "$TMPDIR"/sort_case
assert_cmd_pat 'charlie$' sort -f "$TMPDIR"/sort_case

echo "  ── unique ──"
printf 'apple\nbanana\napple\ncherry\nbanana\n' > "$TMPDIR"/sort_uniq
assert_cmd "$(printf 'apple\nbanana\ncherry\n')" sort -u "$TMPDIR"/sort_uniq

echo "  ── check sorted ──"
printf 'apple\nbanana\ncherry\n' > "$TMPDIR"/sort_ok
assert_cmd "" sort -c "$TMPDIR"/sort_ok

echo "  ── check unsorted ──"
printf 'apple\ncherry\nbanana\n' > "$TMPDIR"/sort_bad
assert_cmd_pat_stderr 'disorder' sort -c "$TMPDIR"/sort_bad
# sort -c should exit non-zero for unsorted input; we rely on test harness
# We check via 'disorder' pattern on stderr

echo "  ── check unique strict ──"
assert_cmd_pat_stderr 'disorder' sort -c -u "$TMPDIR"/sort_uniq

echo "  ── key field sort ──"
printf 'b 3\na 2\na 1\n' > "$TMPDIR"/sort_key
assert_cmd "$(printf 'a 1\na 2\nb 3\n')" sort -k 1,1 -k 2,2n "$TMPDIR"/sort_key

echo "  ── key with separator ──"
printf 'b:3\na:2\nc:1\n' > "$TMPDIR"/sort_t
assert_cmd "$(printf 'a:2\nb:3\nc:1\n')" sort -t : -k 1,1 "$TMPDIR"/sort_t

echo "  ── stable sort ──"
printf 'a 2\na 1\nb 3\n' > "$TMPDIR"/sort_stable
# Stable: equal-key lines keep input order (a 2 before a 1)
assert_cmd "$(printf 'a 2\na 1\nb 3\n')" sort -k 1,1 -s "$TMPDIR"/sort_stable
# Non-stable: tiebreaker by full line (a 1 before a 2)
assert_cmd "$(printf 'a 1\na 2\nb 3\n')" sort -k 1,1 "$TMPDIR"/sort_stable

echo "  ── output to file ──"
printf 'c\na\nb\n' > "$TMPDIR"/sort_in
"$MODBOX" sort -o "$TMPDIR"/sort_out "$TMPDIR"/sort_in 2>/dev/null || true
assert_cmd "$(printf 'a\nb\nc\n')" sort "$TMPDIR"/sort_out

echo "  ── ignore leading blanks ──"
printf '  b\n a\n  a\nc\n' > "$TMPDIR"/sort_blank
assert_cmd "$(printf ' a\n  a\n  b\nc\n')" sort -b "$TMPDIR"/sort_blank

echo "  ── help ──"
assert_cmd_pat 'Usage:' sort --help

echo "  ── stdin ──"
result=$("$MODBOX" sort < "$TMPDIR"/sort_alpha 2>/dev/null || true)
expected="$(printf 'apple\nbanana\ncherry\ndate')"
if [[ "$result" == "$expected" ]]; then
    pass "sort (stdin)"
else
    fail "sort (stdin) — expected [$expected] got [$result]"
fi

echo "  ── multiple files ──"
printf 'c\na\n' > "$TMPDIR"/sort_mf1
printf 'b\nd\n' > "$TMPDIR"/sort_mf2
    assert_cmd "$(printf 'a\nb\nc\nd\n')" sort "$TMPDIR"/sort_mf1 "$TMPDIR"/sort_mf2
