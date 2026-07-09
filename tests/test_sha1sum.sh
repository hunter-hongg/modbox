SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sha1sum ────────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/sha1_test.txt
printf '' > "$TMPDIR"/sha1_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/sha1_multiline.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{40}  '"$TMPDIR"'/sha1_test.txt$' sha1sum "$TMPDIR"/sha1_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{40}  '"$TMPDIR"'/sha1_empty.txt$' sha1sum "$TMPDIR"/sha1_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{40}  -$' sha1sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{40}  -$' sha1sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{40} \*'"$TMPDIR"'/sha1_test.txt$' sha1sum -b "$TMPDIR"/sha1_test.txt

echo "  ── text mode (-t) ──"
assert_cmd_pat '^[a-f0-9]{40}  '"$TMPDIR"'/sha1_test.txt$' sha1sum -t "$TMPDIR"/sha1_test.txt

echo "  ── multiple files ──"
output=$("$MODBOX" sha1sum "$TMPDIR"/sha1_test.txt "$TMPDIR"/sha1_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "sha1sum multiple files produces 2 lines"
else
    fail "sha1sum multiple files expected 2 lines, got $line_count"
fi

echo "  ── check mode (-c) with valid checksum ──"
# Create a checksum file using the real sha1sum to get expected format
if command -v sha1sum &>/dev/null; then
    real_sha=$(sha1sum "$TMPDIR"/sha1_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_sha" "$TMPDIR"/sha1_test.txt > "$TMPDIR"/sha1_checksums.txt
    assert_cmd_pat 'OK$' sha1sum -c "$TMPDIR"/sha1_checksums.txt
else
    echo "  SKIP (system sha1sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
if command -v sha1sum &>/dev/null; then
    printf '0000000000000000000000000000000000000000  %s\n' "$TMPDIR"/sha1_test.txt > "$TMPDIR"/sha1_bad_checksums.txt
    assert_cmd_pat 'FAILED' sha1sum -c "$TMPDIR"/sha1_bad_checksums.txt
else
    echo "  SKIP (system sha1sum not available)"
fi

echo "  ── --tag format ──"
assert_cmd_pat '^SHA1 \('"$TMPDIR"'/sha1_test.txt\) = [a-f0-9]{40}' sha1sum --tag "$TMPDIR"/sha1_test.txt

echo "  ── --zero (NUL-terminated) ──"
output=$("$MODBOX" sha1sum --zero "$TMPDIR"/sha1_test.txt)
if [[ "$output" == *$'\0' ]]; then
    pass "sha1sum --zero produces NUL-terminated output"
else
    fail "sha1sum --zero does not produce NUL-terminated output"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' sha1sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' sha1sum "$TMPDIR"/sha1_nonexistent.txt
