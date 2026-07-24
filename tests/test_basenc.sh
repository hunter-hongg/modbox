SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── basenc ──────────────────────────────────"

printf 'hello world' > "$TMPDIR"/benc_test.txt
printf '' > "$TMPDIR"/benc_empty.txt

# ── base64 ──────────────────────────────────────
echo "  ── --base64 encode ──"
assert_cmd 'aGVsbG8gd29ybGQ=' basenc --base64 "$TMPDIR"/benc_test.txt

echo "  ── --base64 decode ──"
printf 'aGVsbG8gd29ybGQ=' > "$TMPDIR"/benc_b64.txt
assert_cmd 'hello world' basenc --base64 -d "$TMPDIR"/benc_b64.txt

echo "  ── --base64 stdin encode ──"
actual=$(printf 'hello world' | "$MODBOX" basenc --base64 2>/dev/null || true)
if [[ "$actual" == 'aGVsbG8gd29ybGQ=' ]]; then
    pass "basenc --base64 stdin encode"
else
    fail "basenc --base64 stdin encode — expected [aGVsbG8gd29ybGQ=] got [$actual]"
fi

echo "  ── --base64 stdin decode ──"
actual=$(printf 'aGVsbG8gd29ybGQ=' | "$MODBOX" basenc --base64 -d 2>/dev/null || true)
if [[ "$actual" == 'hello world' ]]; then
    pass "basenc --base64 stdin decode"
else
    fail "basenc --base64 stdin decode — expected [hello world] got [$actual]"
fi

# ── base64url ────────────────────────────────────
echo "  ── --base64url encode ──"
assert_cmd 'aGVsbG8gd29ybGQ=' basenc --base64url "$TMPDIR"/benc_test.txt

echo "  ── --base64url decode ──"
assert_cmd 'hello world' basenc --base64url -d "$TMPDIR"/benc_b64.txt

echo "  ── --base64url with special chars ──"
printf '\xff\xfb' > "$TMPDIR"/benc_binary.txt
assert_cmd '__s=' basenc --base64url "$TMPDIR"/benc_binary.txt

# ── base32 ──────────────────────────────────────
echo "  ── --base32 encode ──"
assert_cmd 'NBSWY3DPEB3W64TMMQ======' basenc --base32 "$TMPDIR"/benc_test.txt

echo "  ── --base32 decode ──"
printf 'NBSWY3DPEB3W64TMMQ======' > "$TMPDIR"/benc_b32.txt
assert_cmd 'hello world' basenc --base32 -d "$TMPDIR"/benc_b32.txt

# ── base32hex ────────────────────────────────────
echo "  ── --base32hex encode ──"
assert_cmd 'D1IMOR3F41RMUSJCCG======' basenc --base32hex "$TMPDIR"/benc_test.txt

echo "  ── --base32hex decode ──"
printf 'D1IMOR3F41RMUSJCCG======' > "$TMPDIR"/benc_b32h.txt
assert_cmd 'hello world' basenc --base32hex -d "$TMPDIR"/benc_b32h.txt

# ── base16 ───────────────────────────────────────
echo "  ── --base16 encode ──"
assert_cmd '68656C6C6F20776F726C64' basenc --base16 "$TMPDIR"/benc_test.txt

echo "  ── --base16 decode ──"
printf '68656C6C6F20776F726C64' > "$TMPDIR"/benc_b16.txt
assert_cmd 'hello world' basenc --base16 -d "$TMPDIR"/benc_b16.txt

# ── base2msbf ────────────────────────────────────
echo "  ── --base2msbf encode ──"
printf '\x01' > "$TMPDIR"/benc_b2m.txt
assert_cmd '00000001' basenc --base2msbf "$TMPDIR"/benc_b2m.txt

echo "  ── --base2msbf decode ──"
printf '00000001' > "$TMPDIR"/benc_b2m_dec.txt
assert_cmd "$(printf '\x01')" basenc --base2msbf -d "$TMPDIR"/benc_b2m_dec.txt

# ── base2lsbf ────────────────────────────────────
echo "  ── --base2lsbf encode ──"
assert_cmd '10000000' basenc --base2lsbf "$TMPDIR"/benc_b2m.txt

echo "  ── --base2lsbf decode ──"
printf '10000000' > "$TMPDIR"/benc_b2l_dec.txt
assert_cmd "$(printf '\x01')" basenc --base2lsbf -d "$TMPDIR"/benc_b2l_dec.txt

# ── empty file ───────────────────────────────────
echo "  ── empty file encode ──"
assert_cmd '' basenc --base64 "$TMPDIR"/benc_empty.txt

echo "  ── empty file decode ──"
printf '' > "$TMPDIR"/benc_empty_enc.txt
assert_cmd '' basenc --base64 -d "$TMPDIR"/benc_empty_enc.txt

# ── wrap option ──────────────────────────────────
echo "  ── wrap option (w=0 disables wrapping) ──"
printf '%0.sA' {1..100} > "$TMPDIR"/benc_long.txt
output=$("$MODBOX" basenc --base64 -w 0 "$TMPDIR"/benc_long.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 1 ]]; then
    pass "basenc --base64 -w 0 → single line"
else
    fail "basenc --base64 -w 0 expected 1 line, got $line_count"
fi

echo "  ── wrap option (w=20) ──"
output=$("$MODBOX" basenc --base64 -w 20 "$TMPDIR"/benc_long.txt)
first_line_len=$(echo "$output" | head -1 | wc -c)
if [[ $first_line_len -le 21 ]]; then
    pass "basenc --base64 -w 20 → lines ~20 chars"
else
    fail "basenc --base64 -w 20 expected ~20 chars per line, got $first_line_len"
fi

# ── error cases ──────────────────────────────────
echo "  ── error: no encoding type specified ──"
assert_cmd_pat_stderr 'exactly one encoding type' basenc -d

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' basenc --base64 "$TMPDIR"/benc_nonexistent.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' basenc --help
