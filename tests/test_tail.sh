SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tail ─────────────────────────────────────"

# Create test files
printf 'line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n' > "$TMPDIR"/head_lines
printf '' > "$TMPDIR"/head_empty
# Create NUL-terminated file using Python
python3 -c "import sys; sys.stdout.buffer.write(b'a\x00b\x00c\x00')" > "$TMPDIR"/head_nul

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
# Check that tail -z -n 2 produces output (NUL bytes are hard to compare in bash)
if python3 -c "import sys; sys.stdout.buffer.write(b'a\x00b\x00c\x00')" | "$MODBOX" tail -z -n 2 2>/dev/null | python3 -c "import sys; data = sys.stdin.buffer.read(); sys.exit(0 if data == b'b\x00c\x00' else 1)"; then
    pass "tail -z -n 2"
else
    fail "tail -z -n 2 — expected NUL-terminated output"
fi

echo "  ── -n +N beyond EOF (produce nothing) ──"
assert_cmd "" tail -n +99 "$TMPDIR"/head_lines
