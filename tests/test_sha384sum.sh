SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sha384sum ───────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/sha384_test.txt
printf '' > "$TMPDIR"/sha384_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/sha384_multiline.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{96}  '"$TMPDIR"'/sha384_test.txt$' sha384sum "$TMPDIR"/sha384_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{96}  '"$TMPDIR"'/sha384_empty.txt$' sha384sum "$TMPDIR"/sha384_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{96}  -$' sha384sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{96}  -$' sha384sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{96} \*'"$TMPDIR"'/sha384_test.txt$' sha384sum -b "$TMPDIR"/sha384_test.txt

echo "  ── text mode (-t) ──"
assert_cmd_pat '^[a-f0-9]{96}  '"$TMPDIR"'/sha384_test.txt$' sha384sum -t "$TMPDIR"/sha384_test.txt

echo "  ── multiple files ──"
output=$("$MODBOX" sha384sum "$TMPDIR"/sha384_test.txt "$TMPDIR"/sha384_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "sha384sum multiple files produces 2 lines"
else
    fail "sha384sum multiple files expected 2 lines, got $line_count"
fi

echo "  ── check mode (-c) with valid checksum ──"
# Create a checksum file using the real sha384sum to get expected format
if command -v sha384sum &>/dev/null; then
    real_sha=$(sha384sum "$TMPDIR"/sha384_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_sha" "$TMPDIR"/sha384_test.txt > "$TMPDIR"/sha384_checksums.txt
    assert_cmd_pat 'OK$' sha384sum -c "$TMPDIR"/sha384_checksums.txt
else
    echo "  SKIP (system sha384sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
if command -v sha384sum &>/dev/null; then
    printf '%s  %s\n' "$(printf '0%.0s' {1..96})" "$TMPDIR"/sha384_test.txt > "$TMPDIR"/sha384_bad_checksums.txt
    assert_cmd_pat 'FAILED' sha384sum -c "$TMPDIR"/sha384_bad_checksums.txt
else
    echo "  SKIP (system sha384sum not available)"
fi

echo "  ── --tag format ──"
assert_cmd_pat '^SHA384 \('"$TMPDIR"'/sha384_test.txt\) = [a-f0-9]{96}' sha384sum --tag "$TMPDIR"/sha384_test.txt

echo "  ── --zero (NUL-terminated) ──"
output=$("$MODBOX" sha384sum --zero "$TMPDIR"/sha384_test.txt)
if [[ "$output" == *$'\0' ]]; then
    pass "sha384sum --zero produces NUL-terminated output"
else
    fail "sha384sum --zero does not produce NUL-terminated output"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' sha384sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' sha384sum "$TMPDIR"/sha384_nonexistent.txt
