echo ""
echo "── md5sum ────────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/md5_test.txt
printf '' > "$TMPDIR"/md5_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/md5_multiline.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{32}  '"$TMPDIR"'/md5_test.txt$' md5sum "$TMPDIR"/md5_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{32}  '"$TMPDIR"'/md5_empty.txt$' md5sum "$TMPDIR"/md5_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{32}  -$' md5sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{32}  -$' md5sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{32} \*'"$TMPDIR"'/md5_test.txt$' md5sum -b "$TMPDIR"/md5_test.txt

echo "  ── text mode (-t) ──"
assert_cmd_pat '^[a-f0-9]{32}  '"$TMPDIR"'/md5_test.txt$' md5sum -t "$TMPDIR"/md5_test.txt

echo "  ── multiple files ──"
output=$("$MODBOX" md5sum "$TMPDIR"/md5_test.txt "$TMPDIR"/md5_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "md5sum multiple files produces 2 lines"
else
    fail "md5sum multiple files expected 2 lines, got $line_count"
fi

echo "  ── check mode (-c) with valid checksum ──"
# Create a checksum file using the real md5sum to get expected format
if command -v md5sum &>/dev/null; then
    real_md5=$(md5sum "$TMPDIR"/md5_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_md5" "$TMPDIR"/md5_test.txt > "$TMPDIR"/md5_checksums.txt
    assert_cmd_pat 'OK$' md5sum -c "$TMPDIR"/md5_checksums.txt
else
    echo "  SKIP (system md5sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
if command -v md5sum &>/dev/null; then
    printf '00000000000000000000000000000000  %s\n' "$TMPDIR"/md5_test.txt > "$TMPDIR"/md5_bad_checksums.txt
    assert_cmd_pat 'FAILED' md5sum -c "$TMPDIR"/md5_bad_checksums.txt
else
    echo "  SKIP (system md5sum not available)"
fi

echo "  ── --tag format ──"
assert_cmd_pat '^MD5 \('"$TMPDIR"'/md5_test.txt\) = [a-f0-9]{32}' md5sum --tag "$TMPDIR"/md5_test.txt

echo "  ── --zero (NUL-terminated) ──"
output=$("$MODBOX" md5sum --zero "$TMPDIR"/md5_test.txt)
if [[ "$output" == *$'\0' ]]; then
    pass "md5sum --zero produces NUL-terminated output"
else
    fail "md5sum --zero does not produce NUL-terminated output"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' md5sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' md5sum "$TMPDIR"/md5_nonexistent.txt
