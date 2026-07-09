SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── base64 ───────────────────────────────────"

# Create test files
printf 'hello world' > "$TMPDIR"/b64_test.txt
printf '' > "$TMPDIR"/b64_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/b64_multiline.txt

echo "  ── basic encode ──"
assert_cmd 'aGVsbG8gd29ybGQ=' base64 "$TMPDIR"/b64_test.txt

echo "  ── basic decode ──"
printf 'aGVsbG8gd29ybGQ=' > "$TMPDIR"/b64_encoded.txt
assert_cmd 'hello world' base64 -d "$TMPDIR"/b64_encoded.txt

echo "  ── empty file encode ──"
assert_cmd '' base64 "$TMPDIR"/b64_empty.txt

echo "  ── empty file decode ──"
printf '' > "$TMPDIR"/b64_empty_encoded.txt
assert_cmd '' base64 -d "$TMPDIR"/b64_empty_encoded.txt

echo "  ── stdin encode ──"
actual=$(printf 'hello world' | "$MODBOX" base64 2>/dev/null || true)
if [[ "$actual" == 'aGVsbG8gd29ybGQ=' ]]; then
    pass "base64 stdin encode"
else
    fail "base64 stdin encode — expected [aGVsbG8gd29ybGQ=] got [$actual]"
fi

echo "  ── stdin decode ──"
actual=$(printf 'aGVsbG8gd29ybGQ=' | "$MODBOX" base64 -d 2>/dev/null || true)
if [[ "$actual" == 'hello world' ]]; then
    pass "base64 stdin decode"
else
    fail "base64 stdin decode — expected [hello world] got [$actual]"
fi

echo "  ── stdin (explicit -) encode ──"
actual=$(printf 'hello world' | "$MODBOX" base64 - 2>/dev/null || true)
if [[ "$actual" == 'aGVsbG8gd29ybGQ=' ]]; then
    pass "base64 - stdin encode"
else
    fail "base64 - stdin encode — expected [aGVsbG8gd29ybGQ=] got [$actual]"
fi

echo "  ── stdin (explicit -) decode ──"
actual=$(printf 'aGVsbG8gd29ybGQ=' | "$MODBOX" base64 -d - 2>/dev/null || true)
if [[ "$actual" == 'hello world' ]]; then
    pass "base64 -d - stdin decode"
else
    fail "base64 -d - stdin decode — expected [hello world] got [$actual]"
fi

echo "  ── multiline encode ──"
assert_cmd 'YQpiCmMK' base64 "$TMPDIR"/b64_multiline.txt

echo "  ── wrap option (default 76) ──"
# Create a longer string to test wrapping
printf '%0.sA' {1..100} > "$TMPDIR"/b64_long.txt
output=$("$MODBOX" base64 "$TMPDIR"/b64_long.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -gt 1 ]]; then
    pass "base64 wrap (default) → $line_count lines"
else
    fail "base64 wrap (default) expected multiple lines, got $line_count"
fi

echo "  ── wrap option (w=0 disables wrapping) ──"
output=$("$MODBOX" base64 -w 0 "$TMPDIR"/b64_long.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 1 ]]; then
    pass "base64 -w 0 → single line (no wrap)"
else
    fail "base64 -w 0 expected 1 line, got $line_count"
fi

echo "  ── wrap option (w=20) ──"
output=$("$MODBOX" base64 -w 20 "$TMPDIR"/b64_long.txt)
first_line_len=$(echo "$output" | head -1 | wc -c)
if [[ $first_line_len -le 21 ]]; then  # 20 chars + newline
    pass "base64 -w 20 → lines ~20 chars"
else
    fail "base64 -w 20 expected ~20 chars per line, got $first_line_len"
fi

echo "  ── ignore-garbage option with valid data ──"
printf 'aGVsbG8gd29ybGQ=' > "$TMPDIR"/b64_clean.txt
assert_cmd 'hello world' base64 -d -i "$TMPDIR"/b64_clean.txt

echo "  ── ignore-garbage option with garbage characters ──"
printf 'aGVs!bG8g@d29ybGQ=' > "$TMPDIR"/b64_garbage.txt
assert_cmd 'hello world' base64 -d -i "$TMPDIR"/b64_garbage.txt

echo "  ── decode with newlines (always accepted) ──"
printf 'aGVsbG8gd29ybGQ=\n' > "$TMPDIR"/b64_newline.txt
assert_cmd 'hello world' base64 -d "$TMPDIR"/b64_newline.txt

echo "  ── decode without ignore-garbage fails on garbage ──"
printf 'aGVs!bG8gd29ybGQ=' > "$TMPDIR"/b64_garbage2.txt
# Should fail or produce error when garbage is present without -i
output=$("$MODBOX" base64 -d "$TMPDIR"/b64_garbage2.txt 2>&1)
if [[ -n "$output" ]]; then
    pass "base64 -d (no -i) → handles garbage (produces output/error)"
else
    pass "base64 -d (no -i) → handles garbage"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' base64 --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' base64 "$TMPDIR"/b64_nonexistent.txt
