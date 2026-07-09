SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── comm ──────────────────────────────────────"

printf 'apple\nbanana\ncherry\n' > "$TMPDIR"/comm_f1
printf 'banana\ncherry\ndate\n'   > "$TMPDIR"/comm_f2
printf 'APPLE\nBANANA\n'          > "$TMPDIR"/comm_case1
printf 'apple\nbanana\n'          > "$TMPDIR"/comm_case2
printf 'b\na\n' > "$TMPDIR"/comm_unsorted1
printf 'b\na\n' > "$TMPDIR"/comm_unsorted2

echo "  ── basic comm (3 columns) ──"
# Column 1: lines only in file1, col2: only in file2, col3: in both
assert_cmd_pat 'apple' comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'date'  comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'cherry' comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -1 : suppress column 1 ──"
assert_cmd_not_pat 'apple' comm -1 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'date' comm -1 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -2 : suppress column 2 ──"
assert_cmd_pat 'apple' comm -2 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'date' comm -2 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -3 : suppress column 3 ──"
assert_cmd_pat 'apple' comm -3 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'date'  comm -3 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'cherry' comm -3 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -12 : show only column 3 ──"
assert_cmd_pat 'cherry' comm -12 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'apple' comm -12 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'date'  comm -12 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -13 : show only column 2 ──"
assert_cmd_pat 'date' comm -13 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'cherry' comm -13 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -23 : show only column 1 ──"
assert_cmd_pat 'apple' comm -23 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'cherry' comm -23 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -i : ignore case ──"
# With -i, APPLE matches apple, BANANA matches banana.
# Column 3 prints the line from FILE1 (uppercase by default).
assert_cmd_pat 'APPLE' comm -i "$TMPDIR"/comm_case1 "$TMPDIR"/comm_case2
assert_cmd_pat 'BANANA' comm -i "$TMPDIR"/comm_case1 "$TMPDIR"/comm_case2
# Without -i they would all be in separate columns (case differs)

echo "  ── --output-delimiter ──"
result=$("$MODBOX" comm --output-delimiter='|' "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2 2>/dev/null || true)
if echo "$result" | grep -q '|'; then
    pass "comm --output-delimiter='|' → output contains pipes"
else
    fail "comm --output-delimiter='|' — expected pipes in output, got [$result]"
fi

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' comm "$TMPDIR"/comm_nonexistent "$TMPDIR"/comm_f2

echo "  ── unsorted file error ──"
assert_cmd_pat_stderr 'not in sorted order' comm "$TMPDIR"/comm_unsorted1 "$TMPDIR"/comm_f1
# Both need to be sorted, so unsorted2 should also trigger error
assert_cmd_pat_stderr 'not in sorted order' comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_unsorted2

echo "  ── --nocheck-order (skip sort check) ──"
# With --nocheck-order, unsorted input should not produce an error
result=$("$MODBOX" comm --nocheck-order "$TMPDIR"/comm_unsorted1 "$TMPDIR"/comm_unsorted2 2>/dev/null || true)
if [[ -n "$result" && "$result" != *"not in sorted order"* ]]; then
    pass "comm --nocheck-order → no sort error (output produced)"
else
    fail "comm --nocheck-order — expected output without sort error, got [$result]"
fi

echo "  ── stdin via - ──"
printf 'banana\ncherry\n' | assert_cmd_pat 'apple' comm "$TMPDIR"/comm_f1 - 2>/dev/null

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' comm --help
