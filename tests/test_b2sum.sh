SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── b2sum ────────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/b2_test.txt
printf '' > "$TMPDIR"/b2_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/b2_multiline.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{128}  '"$TMPDIR"'/b2_test.txt$' b2sum "$TMPDIR"/b2_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{128}  '"$TMPDIR"'/b2_empty.txt$' b2sum "$TMPDIR"/b2_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{128}  -$' b2sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{128}  -$' b2sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{128} \*'"$TMPDIR"'/b2_test.txt$' b2sum -b "$TMPDIR"/b2_test.txt

echo "  ── text mode (-t) ──"
assert_cmd_pat '^[a-f0-9]{128}  '"$TMPDIR"'/b2_test.txt$' b2sum -t "$TMPDIR"/b2_test.txt

echo "  ── length option (-l 256) ──"
assert_cmd_pat '^[a-f0-9]{64}  '"$TMPDIR"'/b2_test.txt$' b2sum -l 256 "$TMPDIR"/b2_test.txt

echo "  ── multiple files ──"
output=$("$MODBOX" b2sum "$TMPDIR"/b2_test.txt "$TMPDIR"/b2_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "b2sum multiple files produces 2 lines"
else
    fail "b2sum multiple files expected 2 lines, got $line_count"
fi

echo "  ── check mode (-c) with valid checksum ──"
if command -v b2sum &>/dev/null; then
    real_b2=$(b2sum "$TMPDIR"/b2_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_b2" "$TMPDIR"/b2_test.txt > "$TMPDIR"/b2_checksums.txt
    assert_cmd_pat 'OK$' b2sum -c "$TMPDIR"/b2_checksums.txt
else
    echo "  SKIP (system b2sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
if command -v b2sum &>/dev/null; then
    printf '00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000  %s\n' "$TMPDIR"/b2_test.txt > "$TMPDIR"/b2_bad_checksums.txt
    assert_cmd_pat 'FAILED' b2sum -c "$TMPDIR"/b2_bad_checksums.txt
else
    echo "  SKIP (system b2sum not available)"
fi

echo "  ── --tag format ──"
assert_cmd_pat '^BLAKE2 \('"$TMPDIR"'/b2_test.txt\) = [a-f0-9]{128}' b2sum --tag "$TMPDIR"/b2_test.txt

echo "  ── --zero (NUL-terminated) ──"
output=$("$MODBOX" b2sum --zero "$TMPDIR"/b2_test.txt)
if [[ "$output" == *$'\0' ]]; then
    pass "b2sum --zero produces NUL-terminated output"
else
    fail "b2sum --zero does not produce NUL-terminated output"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' b2sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' b2sum "$TMPDIR"/b2_nonexistent.txt
