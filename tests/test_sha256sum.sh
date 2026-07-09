echo ""
echo "── sha256sum ───────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/sha_test.txt
printf '' > "$TMPDIR"/sha_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/sha_multiline.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{64}  '"$TMPDIR"'/sha_test.txt$' sha256sum "$TMPDIR"/sha_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{64}  '"$TMPDIR"'/sha_empty.txt$' sha256sum "$TMPDIR"/sha_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{64}  -$' sha256sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{64}  -$' sha256sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{64} \*'"$TMPDIR"'/sha_test.txt$' sha256sum -b "$TMPDIR"/sha_test.txt

echo "  ── text mode (-t) ──"
assert_cmd_pat '^[a-f0-9]{64}  '"$TMPDIR"'/sha_test.txt$' sha256sum -t "$TMPDIR"/sha_test.txt

echo "  ── multiple files ──"
output=$("$MODBOX" sha256sum "$TMPDIR"/sha_test.txt "$TMPDIR"/sha_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "sha256sum multiple files produces 2 lines"
else
    fail "sha256sum multiple files expected 2 lines, got $line_count"
fi

echo "  ── check mode (-c) with valid checksum ──"
# Create a checksum file using the real sha256sum to get expected format
if command -v sha256sum &>/dev/null; then
    real_sha=$(sha256sum "$TMPDIR"/sha_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_sha" "$TMPDIR"/sha_test.txt > "$TMPDIR"/sha_checksums.txt
    assert_cmd_pat 'OK$' sha256sum -c "$TMPDIR"/sha_checksums.txt
else
    echo "  SKIP (system sha256sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
if command -v sha256sum &>/dev/null; then
    printf '0000000000000000000000000000000000000000000000000000000000000000  %s\n' "$TMPDIR"/sha_test.txt > "$TMPDIR"/sha_bad_checksums.txt
    assert_cmd_pat 'FAILED' sha256sum -c "$TMPDIR"/sha_bad_checksums.txt
else
    echo "  SKIP (system sha256sum not available)"
fi

echo "  ── --tag format ──"
assert_cmd_pat '^SHA256 \('"$TMPDIR"'/sha_test.txt\) = [a-f0-9]{64}' sha256sum --tag "$TMPDIR"/sha_test.txt

echo "  ── --zero (NUL-terminated) ──"
output=$("$MODBOX" sha256sum --zero "$TMPDIR"/sha_test.txt)
if [[ "$output" == *$'\0' ]]; then
    pass "sha256sum --zero produces NUL-terminated output"
else
    fail "sha256sum --zero does not produce NUL-terminated output"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' sha256sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' sha256sum "$TMPDIR"/sha_nonexistent.txt
