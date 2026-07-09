SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── expand ────────────────────────────────────"

printf '\thello\n'        > "$TMPDIR"/expand_simple.txt
printf 'a\tb\n'           > "$TMPDIR"/expand_mid.txt
printf '\t\thello\n'      > "$TMPDIR"/expand_two.txt
printf '\thello\tworld\n' > "$TMPDIR"/expand_initial.txt
: > "$TMPDIR"/expand_empty.txt


echo "  ── basic expand (tab → 8 spaces) ──"
assert_cmd "$(printf '        hello\n')" expand "$TMPDIR"/expand_simple.txt

echo "  ── expand tab mid-line ──"
assert_cmd "$(printf 'a       b\n')" expand "$TMPDIR"/expand_mid.txt

echo "  ── expand two tabs ──"
assert_cmd "$(printf '                hello\n')" expand "$TMPDIR"/expand_two.txt

echo "  ── expand -i (initial only: tab after non-blank preserved) ──"
assert_cmd "$(printf '        hello\tworld\n')" expand -i "$TMPDIR"/expand_initial.txt

echo "  ── expand -i keeps tab after non-blank on same line ──"
assert_cmd "$(printf 'a\tb\n')" expand -i "$TMPDIR"/expand_mid.txt

echo "  ── expand -t 4 (custom tab stop) ──"
assert_cmd "$(printf '    hello\n')" expand -t 4 "$TMPDIR"/expand_simple.txt

echo "  ── expand -t 4 mid-line ──"
assert_cmd "$(printf 'a   b\n')" expand -t 4 "$TMPDIR"/expand_mid.txt

echo "  ── expand -t 1,4,8 (custom list) ──"
assert_cmd "$(printf ' hello\n')" expand -t 1,4,8 "$TMPDIR"/expand_simple.txt

echo "  ── expand stdin ──"
assert_cmd "        hello" expand <<< $'\thello'

echo "  ── expand stdin via - ──"
assert_cmd "        hello" expand - <<< $'\thello'

echo "  ── expand multiple files ──"
assert_cmd "$(printf '        hello\n        hello\n')" expand "$TMPDIR"/expand_simple.txt "$TMPDIR"/expand_simple.txt

echo "  ── expand empty file ──"
assert_cmd "" expand "$TMPDIR"/expand_empty.txt

echo "  ── expand --help ──"
assert_cmd_pat 'Usage:' expand --help

echo "  ── expand error: non-existent file ──"
assert_cmd_pat_stderr 'No such file' expand "$TMPDIR"/expand_nonexistent

echo "  ── expand error: invalid tab list ──"
assert_cmd_pat_stderr 'invalid tab stop' expand -t abc "$TMPDIR"/expand_simple.txt

echo ""
echo "── unexpand ──────────────────────────────────"

printf '        hello\n'        > "$TMPDIR"/unexpand_simple.txt
printf 'a       b\n'            > "$TMPDIR"/unexpand_all.txt
printf '    hello\n'            > "$TMPDIR"/unexpand_t4.txt
printf '   hello\n'             > "$TMPDIR"/unexpand_partial.txt
printf '        hello\tworld\n' > "$TMPDIR"/unexpand_initial.txt
printf '        a        b\n'   > "$TMPDIR"/unexpand_two_stops.txt
: > "$TMPDIR"/unexpand_empty.txt

echo "  ── basic unexpand (leading spaces → tabs) ──"
assert_cmd "$(printf '\thello\n')" unexpand "$TMPDIR"/unexpand_simple.txt

echo "  ── unexpand -a (all spaces, not just leading) ──"
assert_cmd "$(printf 'a\tb\n')" unexpand -a "$TMPDIR"/unexpand_all.txt

echo "  ── unexpand -a multiple tab stops ──"
printf '        a       b\n' > "$TMPDIR"/unexpand_two_stops.txt
assert_cmd "$(printf '\ta\tb\n')" unexpand -a "$TMPDIR"/unexpand_two_stops.txt

echo "  ── unexpand -t 4 (custom tab stop) ──"
assert_cmd "$(printf '\thello\n')" unexpand -t 4 "$TMPDIR"/unexpand_t4.txt

echo "  ── unexpand -t 4 mid-line (implies -a, 7 spaces→2 tabs 3+4) ──"
assert_cmd "$(printf 'a\t\tb\n')" unexpand -t 4 "$TMPDIR"/unexpand_all.txt

echo "  ── unexpand -t 2,4,8 with 4 spaces at col 0 → two tabs ──"
assert_cmd "$(printf '\t\thello\n')" unexpand -t 2,4,8 "$TMPDIR"/unexpand_t4.txt

echo "  ── unexpand --first-only (leading spaces only) ──"
assert_cmd "$(printf '\thello\tworld\n')" unexpand --first-only "$TMPDIR"/unexpand_initial.txt

echo "  ── unexpand partial spaces (3 spaces → no tab, not enough) ──"
assert_cmd "$(printf '   hello\n')" unexpand "$TMPDIR"/unexpand_partial.txt

echo "  ── unexpand stdin ──"
assert_cmd "$(printf '\thello')" unexpand <<<"$(printf '        hello')"

echo "  ── unexpand stdin via - ──"
assert_cmd "$(printf '\thello')" unexpand - <<<"$(printf '        hello')"

echo "  ── unexpand multiple files ──"
assert_cmd "$(printf '\thello\n\thello\n')" unexpand "$TMPDIR"/unexpand_simple.txt "$TMPDIR"/unexpand_simple.txt

echo "  ── unexpand empty file ──"
assert_cmd "" unexpand "$TMPDIR"/unexpand_empty.txt

echo "  ── unexpand --help ──"
assert_cmd_pat 'Usage:' unexpand --help

echo "  ── unexpand error: non-existent file ──"
assert_cmd_pat_stderr 'No such file' unexpand "$TMPDIR"/unexpand_nonexistent

echo "  ── unexpand error: invalid tab list ──"
assert_cmd_pat_stderr 'invalid tab stop' unexpand -t abc "$TMPDIR"/unexpand_simple.txt

echo "  ── expand | unexpand -a roundtrip ──"
result=$(expand < "$TMPDIR"/expand_simple.txt 2>/dev/null | "$MODBOX" unexpand -a 2>/dev/null || true)
expected=$(printf '\thello')
if [[ "$result" == "$expected" ]]; then
    pass "expand → unexpand -a roundtrip: tab preserved"
else
    fail "expand → unexpand -a roundtrip — expected [$expected] got [$result]"
fi

echo "  ── unexpand -a | expand roundtrip ──"
result=$(unexpand -a < "$TMPDIR"/unexpand_all.txt 2>/dev/null | "$MODBOX" expand 2>/dev/null || true)
expected=$(printf 'a       b')
if [[ "$result" == "$expected" ]]; then
    pass "unexpand -a → expand roundtrip: spaces preserved"
else
    fail "unexpand -a → expand roundtrip — expected [$expected] got [$result]"
fi
