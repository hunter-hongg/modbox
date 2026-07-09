echo ""
echo "── ptx ──────────────────────────────────────"

echo "  ── basic ptx ──"
printf 'The quick brown fox jumps over the lazy dog\n' > "$TMPDIR"/ptx1.txt
assert_cmd_pat 'fox' ptx "$TMPDIR"/ptx1.txt
assert_cmd_pat 'dog' ptx "$TMPDIR"/ptx1.txt
assert_cmd_pat 'quick' ptx "$TMPDIR"/ptx1.txt

echo "  ── ptx with auto reference ──"
assert_cmd_pat 'ptx1.txt' ptx -A "$TMPDIR"/ptx1.txt

echo "  ── ptx with width option ──"
assert_cmd_pat 'fox' ptx -w 60 "$TMPDIR"/ptx1.txt

echo "  ── ptx from stdin ──"
assert_cmd_pat 'fox' ptx <<<"The quick brown fox jumps over the lazy dog"

echo "  ── ptx help ──"
assert_cmd_pat 'Usage:' ptx --help

echo "  ── ptx non-existent file ──"
assert_cmd_pat_stderr "No such file" ptx "$TMPDIR"/nonexistent.txt
