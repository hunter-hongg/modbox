echo ""
echo "── diff ──────────────────────────────────────"

printf 'a\nb\nc\n' > "$TMPDIR"/diff_a
printf 'a\nd\nc\n' > "$TMPDIR"/diff_b
printf 'a\nb\nc\n' > "$TMPDIR"/diff_identical

echo "  ── normal diff (files differ) ──"
assert_cmd_pat '2c2' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '< b' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '> d' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null

echo "  ── identical files produce no output ──"
assert_cmd "" diff "$TMPDIR"/diff_a "$TMPDIR"/diff_identical

echo "  ── -q : brief (report if files differ) ──"
assert_cmd_pat 'differ' diff -q "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
# -q on identical files should produce no output
assert_cmd "" diff -q "$TMPDIR"/diff_a "$TMPDIR"/diff_identical

echo "  ── -s : report identical files ──"
assert_cmd_pat 'identical' diff -s "$TMPDIR"/diff_a "$TMPDIR"/diff_identical 2>/dev/null

echo "  ── -u : unified format ──"
assert_cmd_pat '^---' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^\+\+\+' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^@@' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^-b' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^\+d' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null

echo "  ── -c : context format ──"
assert_cmd_pat '^\*\*\*' diff -c "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^---' diff -c "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '! b' diff -c "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null

echo "  ── -i : ignore case ──"
printf 'Hello\nWorld\n' > "$TMPDIR"/diff_hi
printf 'hello\nworld\n' > "$TMPDIR"/diff_lo
assert_cmd "" diff -i "$TMPDIR"/diff_hi "$TMPDIR"/diff_lo
assert_cmd_pat '1,2c1,2' diff "$TMPDIR"/diff_hi "$TMPDIR"/diff_lo 2>/dev/null  # without -i, different

echo "  ── -w : ignore all whitespace ──"
printf 'a\nb  c\n' > "$TMPDIR"/diff_ws1
printf 'a\nb c\n' > "$TMPDIR"/diff_ws2
assert_cmd "" diff -w "$TMPDIR"/diff_ws1 "$TMPDIR"/diff_ws2

echo "  ── -b : ignore space change ──"
printf 'a\nb  c\n' > "$TMPDIR"/diff_sp1
printf 'a\nb c\n' > "$TMPDIR"/diff_sp2
assert_cmd "" diff -b "$TMPDIR"/diff_sp1 "$TMPDIR"/diff_sp2

echo "  ── stdin via - ──"
printf 'a\ny\nc\n' | assert_cmd_pat '2c2' diff - "$TMPDIR"/diff_a 2>/dev/null

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_nonexistent

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' diff --help
