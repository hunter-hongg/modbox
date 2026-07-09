SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── rev ──────────────────────────────────────"

echo "  ── basic reverse ──"
printf 'abc\nhello world\n12345\n' > "$TMPDIR"/rev_basic
assert_cmd "$(printf 'cba\ndlrow olleh\n54321\n')" rev "$TMPDIR"/rev_basic

echo "  ── stdin ──"
result=$(printf 'abc\nhello world\n12345\n' | "$MODBOX" rev 2>/dev/null || true)
expected="$(printf 'cba\ndlrow olleh\n54321')"
if [[ "$result" == "$expected" ]]; then
    pass "rev (stdin)"
else
    fail "rev (stdin) — expected [$expected] got [$result]"
fi

echo "  ── single character ──"
printf 'a\n' > "$TMPDIR"/rev_single
assert_cmd "$(printf 'a\n')" rev "$TMPDIR"/rev_single

echo "  ── empty line ──"
printf '\n' > "$TMPDIR"/rev_empty
assert_cmd "$(printf '\n')" rev "$TMPDIR"/rev_empty

echo "  ── palindrome ──"
printf 'racecar\n' > "$TMPDIR"/rev_pal
assert_cmd "$(printf 'racecar\n')" rev "$TMPDIR"/rev_pal

echo "  ── multiple files ──"
printf 'ab\n' > "$TMPDIR"/rev_mf1
printf 'cd\n' > "$TMPDIR"/rev_mf2
assert_cmd "$(printf 'ba\ndc\n')" rev "$TMPDIR"/rev_mf1 "$TMPDIR"/rev_mf2

echo "  ── stdin via - ──"
result=$(printf 'xyz' | "$MODBOX" rev - 2>/dev/null || true)
if [[ "$result" == "zyx" ]]; then
    pass "rev - (stdin via -)"
else
    fail "rev - (stdin via -) — expected [zyx] got [$result]"
fi

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' rev "$TMPDIR"/rev_nonexistent

echo "  ── help ──"
assert_cmd_pat 'Usage:' rev --help
