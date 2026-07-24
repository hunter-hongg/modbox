SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── cksum ────────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/cks_test.txt
printf '' > "$TMPDIR"/cks_empty.txt
printf 'a\nb\nc\n' > "$TMPDIR"/cks_multiline.txt
printf 'hello' > "$TMPDIR"/cks_hello.txt
printf 'test\n' > "$TMPDIR"/cks_test_nl.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[0-9]+ [0-9]+ '"$TMPDIR"'/cks_test.txt' cksum "$TMPDIR"/cks_test.txt

echo "  ── empty file checksum ──"
assert_cmd '4294967295 0 '"$TMPDIR"'/cks_empty.txt' cksum "$TMPDIR"/cks_empty.txt

echo "  ── stdin (no file, no pathname in output) ──"
output=$(printf 'hello' | "$MODBOX" cksum 2>/dev/null || true)
if [[ "$output" == "3287646509 5" ]]; then
    pass "cksum stdin 'hello' → 3287646509 5"
else
    fail "cksum stdin 'hello' — expected [3287646509 5] got [$output]"
fi

echo "  ── stdin (explicit -) ──"
output=$(printf 'hello' | "$MODBOX" cksum - 2>/dev/null || true)
if [[ "$output" == "3287646509 5" ]]; then
    pass "cksum - 'hello' → 3287646509 5"
else
    fail "cksum - 'hello' — expected [3287646509 5] got [$output]"
fi

echo "  ── verify against system cksum ──"
if command -v cksum &>/dev/null; then
    sys_output=$(cksum "$TMPDIR"/cks_test.txt | cut -d' ' -f1,2)
    our_output=$("$MODBOX" cksum "$TMPDIR"/cks_test.txt | cut -d' ' -f1,2)
    if [[ "$sys_output" == "$our_output" ]]; then
        pass "cksum matches system for cks_test.txt"
    else
        fail "cksum mismatch: system [$sys_output] vs ours [$our_output]"
    fi

    sys_output=$(printf 'hello' | cksum 2>/dev/null || true)
    our_output=$(printf 'hello' | "$MODBOX" cksum 2>/dev/null || true)
    if [[ "$sys_output" == "$our_output" ]]; then
        pass "cksum stdin 'hello' matches system"
    else
        fail "cksum stdin 'hello' mismatch: system [$sys_output] vs ours [$our_output]"
    fi
else
    echo "  SKIP (system cksum not available for cross-check)"
fi

echo "  ── multiple files ──"
output=$("$MODBOX" cksum "$TMPDIR"/cks_test.txt "$TMPDIR"/cks_empty.txt)
line_count=$(echo "$output" | wc -l)
if [[ $line_count -eq 2 ]]; then
    pass "cksum multiple files produces 2 lines"
else
    fail "cksum multiple files expected 2 lines, got $line_count"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' cksum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' cksum "$TMPDIR"/cks_nonexistent.txt
