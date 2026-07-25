SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sha512sum ───────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/sha512_test.txt
printf '' > "$TMPDIR"/sha512_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/sha512_multiline.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{128}  '"$TMPDIR"'/sha512_test.txt$' sha512sum "$TMPDIR"/sha512_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{128}  '"$TMPDIR"'/sha512_empty.txt$' sha512sum "$TMPDIR"/sha512_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{128}  -$' sha512sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{128}  -$' sha512sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{128} \*'"$TMPDIR"'/sha512_test.txt$' sha512sum -b "$TMPDIR"/sha512_test.txt

echo "  ── text mode (-t) ──"
assert_cmd_pat '^[a-f0-9]{128}  '"$TMPDIR"'/sha512_test.txt$' sha512sum -t "$TMPDIR"/sha512_test.txt

echo "  ── multiple files ──"
output=$("$MODBOX" sha512sum "$TMPDIR"/sha512_test.txt "$TMPDIR"/sha512_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "sha512sum multiple files produces 2 lines"
else
    fail "sha512sum multiple files expected 2 lines, got $line_count"
fi

echo "  ── check mode (-c) with valid checksum ──"
# Create a checksum file using the real sha512sum to get expected format
if command -v sha512sum &>/dev/null; then
    real_sha=$(sha512sum "$TMPDIR"/sha512_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_sha" "$TMPDIR"/sha512_test.txt > "$TMPDIR"/sha512_checksums.txt
    assert_cmd_pat 'OK$' sha512sum -c "$TMPDIR"/sha512_checksums.txt
else
    echo "  SKIP (system sha512sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
if command -v sha512sum &>/dev/null; then
    printf '%s  %s\n' "$(printf '0%.0s' {1..128})" "$TMPDIR"/sha512_test.txt > "$TMPDIR"/sha512_bad_checksums.txt
    assert_cmd_pat 'FAILED' sha512sum -c "$TMPDIR"/sha512_bad_checksums.txt
else
    echo "  SKIP (system sha512sum not available)"
fi

echo "  ── --tag format ──"
assert_cmd_pat '^SHA512 \('"$TMPDIR"'/sha512_test.txt\) = [a-f0-9]{128}' sha512sum --tag "$TMPDIR"/sha512_test.txt

echo "  ── --zero (NUL-terminated) ──"
output=$("$MODBOX" sha512sum --zero "$TMPDIR"/sha512_test.txt)
if [[ "$output" == *$'\0' ]]; then
    pass "sha512sum --zero produces NUL-terminated output"
else
    fail "sha512sum --zero does not produce NUL-terminated output"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' sha512sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' sha512sum "$TMPDIR"/sha512_nonexistent.txt
