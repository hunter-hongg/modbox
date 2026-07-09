echo ""
echo "── head ─────────────────────────────────────"

printf 'line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n' > "$TMPDIR"/head_lines

echo "  ── default (10 lines) ──"
assert_cmd "$(printf 'line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\n')" head "$TMPDIR"/head_lines

echo "  ── -n N ──"
assert_cmd "$(printf 'line1\nline2\nline3\n')" head -n 3 "$TMPDIR"/head_lines

echo "  ── -c N ──"
assert_cmd "$(printf 'line1\nl')" head -c 7 "$TMPDIR"/head_lines

echo "  ── stdin ──"
result=$(printf 'a\nb\nc\n' | "$MODBOX" head -n 2 2>/dev/null || true)
if [[ "$result" == "$(printf 'a\nb')" ]]; then
    pass "head (stdin)"
else
    fail "head (stdin) — expected [a/b] got [$result]"
fi

echo "  ── multiple files with headers ──"
assert_cmd_pat '==>' head "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── -q: quiet ──"
assert_cmd_not_pat '==>' head -q "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── help ──"
assert_cmd_pat 'Usage:' head --help

echo "  ── empty file ──"
: > "$TMPDIR"/head_empty
assert_cmd "" head "$TMPDIR"/head_empty

echo "  ── -n +N (from line N) ──"
assert_cmd "$(printf 'line5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n')" head -n +5 "$TMPDIR"/head_lines

echo "  ── -z (NUL terminated) ──"
printf 'a\0b\0c\0' > "$TMPDIR"/head_nul
assert_cmd "$(printf 'a\0b\0')" head -z -n 2 "$TMPDIR"/head_nul
