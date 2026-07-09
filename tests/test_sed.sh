echo ""
echo "── sed ───────────────────────────────────────"

# Setup test data
printf 'hello world\nfoo bar\nhello again\n' > "$TMPDIR"/sed_basic.txt
printf 'line1\nline2\nline3\nline4\nline5\n' > "$TMPDIR"/sed_5lines.txt
printf 'one\n' > "$TMPDIR"/sed_one.txt
printf 'apple\nbanana\ncherry\ndurian\n' > "$TMPDIR"/sed_fruits.txt
printf 'HELLO\nHello\nhello\n' > "$TMPDIR"/sed_case.txt
printf 'x\ny\nz\n' > "$TMPDIR"/sed_xyz.txt
printf '\n' > "$TMPDIR"/sed_blank.txt
printf 'test & replace\n' > "$TMPDIR"/sed_ampersand.txt

echo "  ── basic substitution ──"
assert_cmd "hi world" sed 's/hello/hi/' <<<"hello world"

echo "  ── global substitution ──"
assert_cmd "baz bar baz" sed 's/foo/baz/g' <<<"foo bar foo"

echo "  ── -n suppress auto-print ──"
assert_cmd "line2" sed -n 'p' <<<"line2"

echo "  ── -n with p command ──"
assert_cmd "line2" sed -n '2p' "$TMPDIR"/sed_5lines.txt

echo "  ── delete line ──"
assert_cmd "$(printf 'line1\nline3\nline4\nline5\n')" sed '2d' "$TMPDIR"/sed_5lines.txt

echo "  ── last line delete (no crash) ──"
assert_cmd "$(printf 'line1\nline2\nline3\nline4\n')" sed '5d' "$TMPDIR"/sed_5lines.txt

echo "  ── range print ──"
assert_cmd "$(printf 'line2\nline3\nline4\n')" sed -n '2,4p' "$TMPDIR"/sed_5lines.txt

echo "  ── quit command ──"
assert_cmd "$(printf 'hello world\nfoo bar\nhello again\n')" sed '/hello again/q' "$TMPDIR"/sed_basic.txt

echo "  ── append command ──"
assert_cmd "$(printf 'line1\nline2\nAFTER\nline3\nline4\nline5\n')" sed '2a\AFTER' "$TMPDIR"/sed_5lines.txt

echo "  ── insert command ──"
assert_cmd "$(printf 'line1\nBEFORE\nline2\nline3\nline4\nline5\n')" sed '2i\BEFORE' "$TMPDIR"/sed_5lines.txt

echo "  ── change command ──"
assert_cmd "$(printf 'line1\nCHANGED\nline3\nline4\nline5\n')" sed '2c\CHANGED' "$TMPDIR"/sed_5lines.txt

echo "  ── line number command ──"
assert_cmd "2" sed -n '/banana/=' "$TMPDIR"/sed_fruits.txt

echo "  ── transliterate ──"
assert_cmd "HELLO" sed 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' <<<"hello"

echo "  ── last line address ($) ──"
assert_cmd "line5" sed -n '$p' "$TMPDIR"/sed_5lines.txt

echo "  ── multiple -e scripts ──"
assert_cmd "$(printf 'FOO\nBAR\n')" sed -e 's/foo/FOO/' -e 's/bar/BAR/' <<<"$(printf 'foo\nbar\n')"

echo "  ── case-insensitive substitution ──"
assert_cmd "$(printf 'hi\nhi\nhi\n')" sed 's/hello/hi/I' "$TMPDIR"/sed_case.txt

echo "  ── negation ──"
assert_cmd "$(printf 'line1\nline3\nline4\nline5\n')" sed -n '2!p' "$TMPDIR"/sed_5lines.txt

echo "  ── substitution with & reference ──"
assert_cmd "test AND replace" sed 's/&/AND/' "$TMPDIR"/sed_ampersand.txt

echo "  ── backreference \1 (BRE syntax) ──"
assert_cmd "world hello" sed 's/\(hello\) \(world\)/\2 \1/' <<<"hello world"

echo "  ── backreference -E (ERE syntax) ──"
assert_cmd "world hello" sed -E 's/(hello) (world)/\2 \1/' <<<"hello world"

echo "  ── substitution with different delimiter ──"
assert_cmd "/new/path/file" sed 's|/old/path/|/new/path/|' <<<"/old/path/file"

echo "  ── regex address ──"
assert_cmd "$(printf 'apple\nbanana\nCHERRY\ndurian\n')" sed '/cherry/c\CHERRY' "$TMPDIR"/sed_fruits.txt

echo "  ── regex range ──"
assert_cmd "$(printf 'banana\ncherry\n')" sed -n '/banana/,/cherry/p' "$TMPDIR"/sed_fruits.txt

echo "  ── in-place editing (-i) ──"
cp "$TMPDIR"/sed_xyz.txt "$TMPDIR"/sed_inplace.txt
assert_cmd "" sed -i 's/y/Y/' "$TMPDIR"/sed_inplace.txt
assert_cmd "$(printf 'x\nY\nz\n')" cat "$TMPDIR"/sed_inplace.txt

echo "  ── in-place with backup (-i.bak) ──"
cp "$TMPDIR"/sed_xyz.txt "$TMPDIR"/sed_backup.txt
assert_cmd "" sed -i.bak 's/z/ZZZ/' "$TMPDIR"/sed_backup.txt
assert_cmd "$(printf 'x\ny\nZZZ\n')" cat "$TMPDIR"/sed_backup.txt
assert_cmd "$(printf 'x\ny\nz\n')" cat "$TMPDIR"/sed_backup.txt.bak

echo "  ── script from file (-f) ──"
printf 's/foo/BAR/g\n' > "$TMPDIR"/sed_script.txt
assert_cmd "BAR BAR" sed -f "$TMPDIR"/sed_script.txt <<<"foo foo"

echo "  ── write command ──"
assert_cmd "$(printf 'apple\nbanana\ncherry\ndurian\n')" sed '2w '"$TMPDIR"'/sed_write_out.txt' "$TMPDIR"/sed_fruits.txt
assert_cmd "banana" cat "$TMPDIR"/sed_write_out.txt

echo "  ── substitution with p flag (with -n) ──"
assert_cmd "$(printf 'LINE1\nLINE3\nLINE5\n')" sed -n '/[135]/s/line/LINE/p' "$TMPDIR"/sed_5lines.txt

echo "  ── substitution with g flag ──"
assert_cmd "X y X" sed 's/x/X/g' <<<"x y x"

echo "  ── substitution with numeric flag ──"
assert_cmd "foo bar BAR" sed 's/bar/BAR/2' <<<"foo bar bar"

echo "  ── substitution with w flag ──"
rm -f "$TMPDIR"/sed_subst_write.txt
assert_cmd "$(printf 'X\ny\nz\n')" sed 's/x/X/w '"$TMPDIR"'/sed_subst_write.txt' "$TMPDIR"/sed_xyz.txt
assert_cmd "X" cat "$TMPDIR"/sed_subst_write.txt

echo "  ── read command (r) ──"
printf 'READ\n' > "$TMPDIR"/sed_read_test.txt
assert_cmd "$(printf 'apple\nbanana\nREAD\ncherry\ndurian\n')" sed '/banana/r '"$TMPDIR"'/sed_read_test.txt' "$TMPDIR"/sed_fruits.txt

echo "  ── n command (print then read next) ──"
printf 'a\nb\nc\n' | assert_cmd "b" sed -n 'n;p'

echo "  ── N command (append next line, then substitute) ──"
printf 'a\nb\nc\n' | assert_cmd "a b" sed -n 'N;s/\n/ /;p'

echo "  ── stdin ──"
assert_cmd "MODIFIED" sed 's/old/MODIFIED/' <<<"old"

echo "  ── empty input ──"
assert_cmd "" sed 's/foo/bar/' < /dev/null

echo "  ── multiple files ──"
printf 'A\n' > "$TMPDIR"/sed_mfa.txt
printf 'B\n' > "$TMPDIR"/sed_mfb.txt
assert_cmd "$(printf 'PREFIX-A\nPREFIX-B\n')" sed 's/^/PREFIX-/' "$TMPDIR"/sed_mfa.txt "$TMPDIR"/sed_mfb.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' sed --help

echo "  ── error: no script ──"
assert_cmd_pat_stderr 'no script' sed

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' sed 's/foo/bar/' "$TMPDIR"/sed_nonexistent

echo "  ── non-existent -f file ──"
assert_cmd_pat_stderr "can't read" sed -f "$TMPDIR"/sed_nonexistent_script.txt
