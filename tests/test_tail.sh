echo ""
echo "── tail ─────────────────────────────────────"

echo "  ── default (10 lines) ──"
assert_cmd "$(printf 'line3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n')" tail "$TMPDIR"/head_lines

echo "  ── -n N ──"
assert_cmd "$(printf 'line10\nline11\nline12\n')" tail -n 3 "$TMPDIR"/head_lines

echo "  ── -n +N (from line N) ──"
assert_cmd "$(printf 'line5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n')" tail -n +5 "$TMPDIR"/head_lines

echo "  ── -c N (last bytes) ──"
assert_cmd "$(printf 'e12\n')" tail -c 4 "$TMPDIR"/head_lines

echo "  ── stdin ──"
result=$(printf 'a\nb\nc\n' | "$MODBOX" tail -n 2 2>/dev/null || true)
if [[ "$result" == "$(printf 'b\nc')" ]]; then
    pass "tail (stdin)"
else
    fail "tail (stdin) — expected [b/c] got [$result]"
fi

echo "  ── multiple files with headers ──"
assert_cmd_pat '==>' tail "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── -q: quiet ──"
assert_cmd_not_pat '==>' tail -q "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── help ──"
assert_cmd_pat 'Usage:' tail --help

echo "  ── empty file ──"
assert_cmd "" tail "$TMPDIR"/head_empty

echo "  ── -z (NUL terminated) ──"
assert_cmd "$(printf 'b\0c\0')" tail -z -n 2 "$TMPDIR"/head_nul

echo "  ── -n +N beyond EOF (produce nothing) ──"
assert_cmd "" tail -n +99 "$TMPDIR"/head_lines
