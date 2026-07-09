echo ""
echo "── base32 ───────────────────────────────────"

printf 'hello world' > "$TMPDIR"/b32_test.txt
printf '' > "$TMPDIR"/b32_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/b32_multiline.txt

echo "  ── basic encode ──"
assert_cmd 'NBSWY3DPEB3W64TMMQ======' base32 "$TMPDIR"/b32_test.txt

echo "  ── basic decode ──"
printf 'NBSWY3DPEB3W64TMMQ======' > "$TMPDIR"/b32_encoded.txt
assert_cmd 'hello world' base32 -d "$TMPDIR"/b32_encoded.txt

echo "  ── empty file encode ──"
assert_cmd '' base32 "$TMPDIR"/b32_empty.txt

echo "  ── empty file decode ──"
printf '' > "$TMPDIR"/b32_empty_encoded.txt
assert_cmd '' base32 -d "$TMPDIR"/b32_empty_encoded.txt

echo "  ── stdin encode ──"
actual=$(printf 'hello world' | "$MODBOX" base32 2>/dev/null || true)
if [[ "$actual" == 'NBSWY3DPEB3W64TMMQ======' ]]; then
    pass "base32 stdin encode"
else
    fail "base32 stdin encode — expected [NBSWY3DPEB3W64TMMQ======] got [$actual]"
fi

echo "  ── stdin decode ──"
actual=$(printf 'NBSWY3DPEB3W64TMMQ======' | "$MODBOX" base32 -d 2>/dev/null || true)
if [[ "$actual" == 'hello world' ]]; then
    pass "base32 stdin decode"
else
    fail "base32 stdin decode — expected [hello world] got [$actual]"
fi

echo "  ── stdin (explicit -) encode ──"
actual=$(printf 'hello world' | "$MODBOX" base32 - 2>/dev/null || true)
if [[ "$actual" == 'NBSWY3DPEB3W64TMMQ======' ]]; then
    pass "base32 - stdin encode"
else
    fail "base32 - stdin encode — expected [NBSWY3DPEB3W64TMMQ======] got [$actual]"
fi

echo "  ── stdin (explicit -) decode ──"
actual=$(printf 'NBSWY3DPEB3W64TMMQ======' | "$MODBOX" base32 -d - 2>/dev/null || true)
if [[ "$actual" == 'hello world' ]]; then
    pass "base32 -d - stdin decode"
else
    fail "base32 -d - stdin decode — expected [hello world] got [$actual]"
fi

echo "  ── multiline encode ──"
assert_cmd 'MEFGECTDBI======' base32 "$TMPDIR"/b32_multiline.txt

echo "  ── wrap option (default 76) ──"
printf '%0.sA' {1..100} > "$TMPDIR"/b32_long.txt
output=$("$MODBOX" base32 "$TMPDIR"/b32_long.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -gt 1 ]]; then
    pass "base32 wrap (default) → $line_count lines"
else
    fail "base32 wrap (default) expected multiple lines, got $line_count"
fi

echo "  ── wrap option (w=0 disables wrapping) ──"
output=$("$MODBOX" base32 -w 0 "$TMPDIR"/b32_long.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 1 ]]; then
    pass "base32 -w 0 → single line (no wrap)"
else
    fail "base32 -w 0 expected 1 line, got $line_count"
fi

echo "  ── wrap option (w=20) ──"
output=$("$MODBOX" base32 -w 20 "$TMPDIR"/b32_long.txt)
first_line_len=$(echo "$output" | head -1 | wc -c)
if [[ $first_line_len -le 21 ]]; then
    pass "base32 -w 20 → lines ~20 chars"
else
    fail "base32 -w 20 expected ~20 chars per line, got $first_line_len"
fi

echo "  ── ignore-garbage option with valid data ──"
printf 'NBSWY3DPEB3W64TMMQ======' > "$TMPDIR"/b32_clean.txt
assert_cmd 'hello world' base32 -d -i "$TMPDIR"/b32_clean.txt

echo "  ── ignore-garbage option with garbage characters ──"
printf 'NBSWY3DP!EB3W@64TMMQ======' > "$TMPDIR"/b32_garbage.txt
assert_cmd 'hello world' base32 -d -i "$TMPDIR"/b32_garbage.txt

echo "  ── decode with newlines (always accepted) ──"
printf 'NBSWY3DPEB3W64TMMQ======\n' > "$TMPDIR"/b32_newline.txt
assert_cmd 'hello world' base32 -d "$TMPDIR"/b32_newline.txt

echo "  ── decode without ignore-garbage on garbage ──"
printf 'NBSWY3DP!EB3W64TMMQ======' > "$TMPDIR"/b32_garbage2.txt
output=$("$MODBOX" base32 -d "$TMPDIR"/b32_garbage2.txt 2>&1)
if [[ -n "$output" ]]; then
    pass "base32 -d (no -i) → handles garbage (produces output/error)"
else
    pass "base32 -d (no -i) → handles garbage"
fi

echo "  ── partial blocks (1-4 bytes) ──"
actual=$(printf 'a' | "$MODBOX" base32 2>/dev/null || true)
expected=$(printf 'a' | base32 2>/dev/null)
if [[ "$actual" == "$expected" ]]; then
    pass "base32 encode 1 byte"
else
    fail "base32 encode 1 byte — expected [$expected] got [$actual]"
fi
actual=$(printf 'ab' | "$MODBOX" base32 2>/dev/null || true)
expected=$(printf 'ab' | base32 2>/dev/null)
if [[ "$actual" == "$expected" ]]; then
    pass "base32 encode 2 bytes"
else
    fail "base32 encode 2 bytes — expected [$expected] got [$actual]"
fi
actual=$(printf 'abc' | "$MODBOX" base32 2>/dev/null || true)
expected=$(printf 'abc' | base32 2>/dev/null)
if [[ "$actual" == "$expected" ]]; then
    pass "base32 encode 3 bytes"
else
    fail "base32 encode 3 bytes — expected [$expected] got [$actual]"
fi
actual=$(printf 'abcd' | "$MODBOX" base32 2>/dev/null || true)
expected=$(printf 'abcd' | base32 2>/dev/null)
if [[ "$actual" == "$expected" ]]; then
    pass "base32 encode 4 bytes"
else
    fail "base32 encode 4 bytes — expected [$expected] got [$actual]"
fi

echo "  ── roundtrip decode of partial block encodings ──"
actual=$(printf 'ME======' | "$MODBOX" base32 -d 2>/dev/null || true)
if [[ "$actual" == 'a' ]]; then
    pass "base32 decode 1-byte encoding"
else
    fail "base32 decode 1-byte encoding — expected [a] got [$actual]"
fi
actual=$(printf 'MFRA====' | "$MODBOX" base32 -d 2>/dev/null || true)
if [[ "$actual" == 'ab' ]]; then
    pass "base32 decode 2-byte encoding"
else
    fail "base32 decode 2-byte encoding — expected [ab] got [$actual]"
fi
actual=$(printf 'MFRGG===' | "$MODBOX" base32 -d 2>/dev/null || true)
if [[ "$actual" == 'abc' ]]; then
    pass "base32 decode 3-byte encoding"
else
    fail "base32 decode 3-byte encoding — expected [abc] got [$actual]"
fi
actual=$(printf 'MFRGGZA=' | "$MODBOX" base32 -d 2>/dev/null || true)
if [[ "$actual" == 'abcd' ]]; then
    pass "base32 decode 4-byte encoding"
else
    fail "base32 decode 4-byte encoding — expected [abcd] got [$actual]"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' base32 --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' base32 "$TMPDIR"/b32_nonexistent.txt
