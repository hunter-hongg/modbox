echo ""
echo "── tac ──────────────────────────────────────"

printf 'line1\nline2\nline3\n' > "$TMPDIR"/tac_basic
printf 'a\n\n\nc\n' > "$TMPDIR"/tac_blanks
printf 'single line\n' > "$TMPDIR"/tac_single
: > "$TMPDIR"/tac_empty

echo "  ── basic reverse lines ──"
assert_cmd "$(printf 'line3\nline2\nline1\n')" tac "$TMPDIR"/tac_basic

echo "  ── single line ──"
assert_cmd "$(printf 'single line\n')" tac "$TMPDIR"/tac_single

echo "  ── empty file ──"
assert_cmd "" tac "$TMPDIR"/tac_empty

echo "  ── stdin ──"
result=$(printf 'a\nb\nc\n' | "$MODBOX" tac 2>/dev/null || true)
expected="$(printf 'c\nb\na')"
if [[ "$result" == "$expected" ]]; then
    pass "tac (stdin)"
else
    fail "tac (stdin) — expected [$expected] got [$result]"
fi

echo "  ── multiple files ──"
printf '1\n2\n' > "$TMPDIR"/tac_mf1
printf 'a\nb\n' > "$TMPDIR"/tac_mf2
assert_cmd "$(printf '2\n1\nb\na\n')" tac "$TMPDIR"/tac_mf1 "$TMPDIR"/tac_mf2

echo "  ── -s : custom separator (trailing) ──"
# With -s, each record includes the separator. Reversing reverses the records.
printf 'a:b:c:' > "$TMPDIR"/tac_sep
assert_cmd "$(printf 'c:b:a:')" tac -s : "$TMPDIR"/tac_sep

echo "  ── -b : before mode (separator before each chunk) ──"
printf 'a:b:c:' > "$TMPDIR"/tac_before
assert_cmd "$(printf ':c:ba')" tac -b -s : "$TMPDIR"/tac_before

echo "  ── -r : regex separator ──"
printf 'aXXbXXc' > "$TMPDIR"/tac_regex
assert_cmd "$(printf 'cbXXaXX')" tac -r -s 'X+' "$TMPDIR"/tac_regex

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' tac "$TMPDIR"/tac_nonexistent

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' tac --help

echo "  ── blank lines reversed ──"
assert_cmd "$(printf 'c\n\n\na\n')" tac "$TMPDIR"/tac_blanks
