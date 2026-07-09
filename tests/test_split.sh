echo ""
echo "── split ────────────────────────────────────"

printf 'line1\nline2\nline3\nline4\nline5\n' > "$TMPDIR"/split5.txt
printf 'line1\nline2\n' > "$TMPDIR"/split_2lines.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' split --help

echo "  ── default: 1000 lines, single file ──"
cd "$TMPDIR"
rm -f xaa
"$MODBOX" split split5.txt 2>/dev/null
assert_cmd "$(printf 'line1\nline2\nline3\nline4\nline5\n')" cat xaa
rm -f xaa
cd - > /dev/null

echo "  ── -l 2 lines, alpha suffixes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat xaa
assert_cmd "$(printf 'line3\nline4\n')" cat xab
assert_cmd "$(printf 'line5\n')" cat xac
rm -f xaa xab xac
cd - > /dev/null

echo "  ── -l 2, -d numeric suffixes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -d 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat x00
assert_cmd "$(printf 'line3\nline4\n')" cat x01
rm -f x00 x01 x02
cd - > /dev/null

echo "  ── --verbose ──"
cd "$TMPDIR"
assert_cmd_pat_stderr 'creating file' split split5.txt -l 5 --verbose
rm -f xaa
cd - > /dev/null

echo "  ── -b 8 bytes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -b 8 -d 2>/dev/null
assert_cmd "$(printf 'line1\nli')" cat x00
assert_cmd "$(printf 'ne2\nline')" cat x01
assert_cmd "$(printf '3\nline4\n')" cat x02
assert_cmd "$(printf 'line5\n')" cat x03
rm -f x00 x01 x02 x03
cd - > /dev/null

echo "  ── custom prefix ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -d mypre 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat mypre00
assert_cmd "$(printf 'line3\nline4\n')" cat mypre01
rm -f mypre00 mypre01 mypre02
cd - > /dev/null

echo "  ── -a suffix length ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -d -a 3 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat x000
rm -f x000 x001 x002
cd - > /dev/null

echo "  ── stdin ──"
cd "$TMPDIR"
printf 'a\nb\nc\n' | "$MODBOX" split -l 2 -d 2>/dev/null
assert_cmd "$(printf 'a\nb\n')" cat x00
assert_cmd "$(printf 'c\n')" cat x01
rm -f x00 x01
cd - > /dev/null

echo "  ── -n N (chunks by bytes) ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -n 2 -d 2>/dev/null
assert_cmd "$(printf 'line1\nline2\nlin')" cat x00
assert_cmd "$(printf 'e3\nline4\nline5\n')" cat x01
rm -f x00 x01
cd - > /dev/null

echo "  ── -n l/N (chunks by lines) ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -n l/2 -d 2>/dev/null
assert_cmd "$(printf 'line1\nline2\nline3\n')" cat x00
assert_cmd "$(printf 'line4\nline5\n')" cat x01
rm -f x00 x01
cd - > /dev/null

echo "  ── -C line-bytes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -C 6 -d 2>/dev/null
# each line is 6 bytes ("lineN\n"), so each file gets one line
assert_cmd "$(printf 'line1\n')" cat x00
assert_cmd "$(printf 'line2\n')" cat x01
assert_cmd "$(printf 'line3\n')" cat x02
assert_cmd "$(printf 'line4\n')" cat x03
assert_cmd "$(printf 'line5\n')" cat x04
rm -f x00 x01 x02 x03 x04
cd - > /dev/null

echo "  ── --additional-suffix ──"
cd "$TMPDIR"
"$MODBOX" split split_2lines.txt -l 1 -d --additional-suffix=.txt 2>/dev/null
assert_cmd "$(printf 'line1\n')" cat x00.txt
assert_cmd "$(printf 'line2\n')" cat x01.txt
rm -f x00.txt x01.txt
cd - > /dev/null

echo "  ── -x hex suffixes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -x 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat x00
assert_cmd "$(printf 'line3\nline4\n')" cat x01
rm -f x00 x01 x02
cd - > /dev/null
