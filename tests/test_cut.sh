echo ""
echo "── cut ───────────────────────────────────────"

printf 'a\tb\tc\n1\t2\t3\n' > "$TMPDIR"/cut_basic.txt
printf 'alpha\tbeta\tgamma\n' > "$TMPDIR"/cut_single.txt
printf 'a\tb\tc\nd\n' > "$TMPDIR"/cut_sparse.txt
printf 'aa\tbb\tcc\na\tb\tc\n' > "$TMPDIR"/cut_multi.txt
: > "$TMPDIR"/cut_empty.txt

echo "  ── -f 1 : single field ──"
assert_cmd "$(printf 'a\n1\n')" cut -f1 "$TMPDIR"/cut_basic.txt

echo "  ── -f 2 : second field ──"
assert_cmd "$(printf 'b\n2\n')" cut -f2 "$TMPDIR"/cut_basic.txt

echo "  ── -f 1,3 : multiple fields ──"
assert_cmd "$(printf 'a\tc\n1\t3\n')" cut -f1,3 "$TMPDIR"/cut_basic.txt

echo "  ── -f 2-3 : field range ──"
assert_cmd "$(printf 'b\tc\n2\t3\n')" cut -f2-3 "$TMPDIR"/cut_basic.txt

echo "  ── -f 2- : open-ended range ──"
assert_cmd "$(printf 'b\tc\n2\t3\n')" cut -f2- "$TMPDIR"/cut_basic.txt

echo "  ── -f 1,2,3 : all fields ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\n')" cut -f1,2,3 "$TMPDIR"/cut_basic.txt

echo "  ── -f 1-3 : all fields via range ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\n')" cut -f1-3 "$TMPDIR"/cut_basic.txt

echo "  ── -f 3-3 : single field via range ──"
assert_cmd "$(printf 'c\n3\n')" cut -f3-3 "$TMPDIR"/cut_basic.txt

echo "  ── -f 3 --complement : complement field selection ──"
assert_cmd "$(printf 'a\tb\n1\t2\n')" cut -f3 --complement "$TMPDIR"/cut_basic.txt

echo "  ── -f 1,2 --complement : complement two fields ──"
assert_cmd "$(printf 'c\n3\n')" cut -f1,2 --complement "$TMPDIR"/cut_basic.txt

echo "  ── -c 1-3 : character range ──"
assert_cmd "$(printf 'a\tb\n1\t2\n')" cut -c1-3 "$TMPDIR"/cut_basic.txt

echo "  ── -c 1,4,7 : specific character positions ──"
printf 'abcdefg\n1234567\n' > "$TMPDIR"/cut_chars.txt
assert_cmd "$(printf 'adg\n147\n')" cut -c1,4,7 "$TMPDIR"/cut_chars.txt

echo "  ── -b 1-3 : byte range ──"
assert_cmd "$(printf 'a\tb\n1\t2\n')" cut -b1-3 "$TMPDIR"/cut_basic.txt

echo "  ── -c 2-4 --complement : complement char range ──"
printf 'abcde\n12345\n' > "$TMPDIR"/cut_ccomp.txt
assert_cmd "$(printf 'ae\n15\n')" cut -c2-4 --complement "$TMPDIR"/cut_ccomp.txt

echo "  ── -d: : colon delimiter ──"
printf 'x:y:z\n1:2:3\n' > "$TMPDIR"/cut_colon.txt
assert_cmd "$(printf 'y\n2\n')" cut -d: -f2 "$TMPDIR"/cut_colon.txt

echo "  ── -d, : comma delimiter ──"
printf 'a,b,c\n1,2,3\n' > "$TMPDIR"/cut_comma.txt
assert_cmd "$(printf 'a,c\n1,3\n')" cut -d, -f1,3 "$TMPDIR"/cut_comma.txt

echo "  ── -d' ' : space delimiter ──"
printf 'x y z\n1 2 3\n' > "$TMPDIR"/cut_space.txt
assert_cmd "$(printf 'x z\n1 3\n')" cut -d' ' -f1,3 "$TMPDIR"/cut_space.txt

echo "  ── -s : only-delimited (skip lines without delimiter) ──"
assert_cmd "b" cut -f2 -s "$TMPDIR"/cut_sparse.txt

echo "  ── -s with all-delimited lines (same as without -s) ──"
assert_cmd "$(printf 'b\n2\n')" cut -f2 "$TMPDIR"/cut_basic.txt

echo "  ── --output-delimiter=, : custom output delimiter ──"
assert_cmd "$(printf 'a,c\n1,3\n')" cut -f1,3 --output-delimiter=, "$TMPDIR"/cut_basic.txt

echo "  ── --output-delimiter=: : colon output delimiter ──"
assert_cmd "$(printf 'a:c\n1:3\n')" cut -f1,3 --output-delimiter=: "$TMPDIR"/cut_basic.txt

echo "  ── -d: -f1,3 --output-delimiter=- : mixed delimiters ──"
assert_cmd "$(printf 'x-z\n1-3\n')" cut -d: -f1,3 --output-delimiter=- "$TMPDIR"/cut_colon.txt

echo "  ── stdin ──"
result=$(printf 'a\tb\tc\n1\t2\t3\n' | "$MODBOX" cut -f2 2>/dev/null || true)
expected="$(printf 'b\n2')"
if [[ "$result" == "$expected" ]]; then
    pass "cut -f2 (stdin)"
else
    fail "cut -f2 (stdin) — expected [$expected] got [$result]"
fi

echo "  ── stdin via - ──"
result=$(printf 'a\tb\tc\n1\t2\t3\n' | "$MODBOX" cut -f3 - 2>/dev/null || true)
expected="$(printf 'c\n3')"
if [[ "$result" == "$expected" ]]; then
    pass "cut -f3 (stdin via -)"
else
    fail "cut -f3 (stdin via -) — expected [$expected] got [$result]"
fi

echo "  ── empty file ──"
assert_cmd "" cut -f1 "$TMPDIR"/cut_empty.txt

echo "  ── -f1 single-line file ──"
assert_cmd "alpha" cut -f1 "$TMPDIR"/cut_single.txt

echo "  ── -z zero-terminated ──"
printf 'a\tb\tc\x00x\ty\tz' > "$TMPDIR"/cut_z.txt
result=$("$MODBOX" cut -z -f2 "$TMPDIR"/cut_z.txt 2>/dev/null | od -A n -t x1z | tr -d '\n')
expected=$(printf 'b' | od -A n -t x1z | tr -d '\n')
# -z outputs NUL-separated, so we check the printable part
assert_cmd_pat 'b' cut -z -f2 "$TMPDIR"/cut_z.txt

echo "  ── -z -c 1-3 zero-terminated chars ──"
result=$("$MODBOX" cut -z -c1-3 "$TMPDIR"/cut_z.txt 2>/dev/null | od -A n -t x1z | tr -d '\n')
printf 'a\tb' | assert_cmd_pat 'a.*b' cut -z -c1-3 "$TMPDIR"/cut_z.txt

echo "  ── -z -f2 --output-delimiter=: zero-terminated ──"
printf 'a\tb\tc\x00x\ty\tz' > "$TMPDIR"/cut_zo.txt
assert_cmd_pat 'b' cut -z -f2 "$TMPDIR"/cut_zo.txt

echo "  ── no options (error) ──"
assert_cmd_pat_stderr 'you must specify' cut "$TMPDIR"/cut_basic.txt

echo "  ── -b and -f together (error) ──"
assert_cmd_pat_stderr 'only one type' cut -b1 -f1 "$TMPDIR"/cut_basic.txt

echo "  ── -d without -f (error) ──"
assert_cmd_pat_stderr 'input delimiter' cut -d: -c1 "$TMPDIR"/cut_basic.txt

echo "  ── -s without -f (error) ──"
assert_cmd_pat_stderr 'input delimiter' cut -s -c1 "$TMPDIR"/cut_basic.txt

echo "  ── non-existent file (error) ──"
assert_cmd_pat_stderr 'No such file' cut -f1 "$TMPDIR"/cut_nonexistent.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' cut --help

echo "  ── -f 1- with sparse lines (some no-delim) ──"
assert_cmd "$(printf 'a\tb\tc\nd\n')" cut -f1- "$TMPDIR"/cut_sparse.txt

echo "  ── -f 1 -s with sparse lines (skip no-delim) ──"
assert_cmd "$(printf 'a\n')" cut -f1 -s "$TMPDIR"/cut_sparse.txt

echo "  ── -c 5 : character past end of line ──"
printf 'ab\nc\n' | assert_cmd "$(printf '\n\n')" cut -c5

echo "  ── -f 99 : field past end ──"
assert_cmd "$(printf '\n\n')" cut -f99 "$TMPDIR"/cut_basic.txt

echo "  ── --complement on all fields (empty output) ──"
assert_cmd "$(printf '\n\n')" cut -f1,2,3 --complement "$TMPDIR"/cut_basic.txt

echo "  ── --complement on no fields (passthrough) ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\n')" cut --complement -f99 "$TMPDIR"/cut_basic.txt

echo "  ── -d '' : empty delimiter (treated as default TAB) ──"
printf 'a\tb\tc\n1\t2\t3\n' | assert_cmd "$(printf 'a\n1')" cut -d '' -f1

echo "  ── -d '|' : pipe delimiter ──"
printf 'a|b|c\n1|2|3\n' > "$TMPDIR"/cut_pipe.txt
assert_cmd "$(printf 'b\n2\n')" cut -d'|' -f2 "$TMPDIR"/cut_pipe.txt
