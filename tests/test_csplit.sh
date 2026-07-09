echo ""
echo "── csplit ───────────────────────────────────"

printf 'a\nb\nc\nd\ne\nf\ng\n' > "$TMPDIR"/csplit7.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' csplit --help

echo "  ── /REGEXP/ (include match) ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat xx00
assert_cmd "$(printf 'e\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── %%REGEXP%% (exclude match) ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt %c% 2>/dev/null
assert_cmd "$(printf 'a\nb\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── line number ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt 3 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── multiple patterns ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt /c/ /f/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\n')" cat xx01
assert_cmd "$(printf 'g\n')" cat xx02
rm -f xx00 xx01 xx02
cd - > /dev/null

echo "  ── {N} repeat ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt /c/ '{1}' 2>/dev/null
# /c/ matches line 3, then repeats once, matches /c/ again? no, there's only one "c"
# so it splits at /c/ (line 3) then tries again and fails, remainder goes to last
assert_cmd "$(printf 'a\nb\nc\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── -f prefix ──"
cd "$TMPDIR"
"$MODBOX" csplit -f mypre csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat mypre00
assert_cmd "$(printf 'e\nf\ng\n')" cat mypre01
rm -f mypre00 mypre01
cd - > /dev/null

echo "  ── -n digits ──"
cd "$TMPDIR"
"$MODBOX" csplit -n 3 csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat xx000
assert_cmd "$(printf 'e\nf\ng\n')" cat xx001
rm -f xx000 xx001
cd - > /dev/null

echo "  ── -b suffix-format ──"
cd "$TMPDIR"
"$MODBOX" csplit -b '%03d' csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat xx000
assert_cmd "$(printf 'e\nf\ng\n')" cat xx001
rm -f xx000 xx001
cd - > /dev/null

echo "  ── -s quiet (no size output) ──"
cd "$TMPDIR"
quiet_out=$("$MODBOX" csplit -s csplit7.txt /d/ 2>/dev/null)
if [[ -z "$quiet_out" ]]; then
    pass "csplit -s csplit7.txt /d/"
else
    fail "csplit -s csplit7.txt /d/ — expected empty stdout, got [$quiet_out]"
fi
rm -f xx00 xx01
cd - > /dev/null
