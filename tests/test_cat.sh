SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── cat ─────────────────────────────────────"

printf 'hello\nworld\n' > "$TMPDIR"/simple.txt
printf '\n\n\n'                > "$TMPDIR"/blanks.txt
printf 'line1\n\n\nline4\n'    > "$TMPDIR"/mixed.txt
printf 'with\ttab\n'           > "$TMPDIR"/tab.txt
printf 'a\n\n\nb\n'            > "$TMPDIR"/squeeze.txt
printf 'hello\n'               > "$TMPDIR"/a.txt
printf 'world\n'               > "$TMPDIR"/b.txt
printf '\x7f\n'                > "$TMPDIR"/del.txt
printf '\x01\x02\x1f\n'        > "$TMPDIR"/low.txt
printf '\x80\x9f\xa0\xff\n'    > "$TMPDIR"/high.txt
printf ''                      > "$TMPDIR"/empty.txt

echo "  ── basic read ──"
assert_cmd "$(printf 'hello\nworld\n')" cat "$TMPDIR"/simple.txt

echo "  ── -n : number all lines ──"
assert_cmd "$(printf '     1  hello\n     2  world\n')" cat -n "$TMPDIR"/simple.txt

echo "  ── -b : number non-blank lines ──"
assert_cmd "$(printf '     1  line1\n\n\n     2  line4\n')" cat -b "$TMPDIR"/mixed.txt

echo "  ── -nb : -b overrides -n ──"
assert_cmd "$(printf '     1  line1\n\n\n     2  line4\n')" cat -nb "$TMPDIR"/mixed.txt

echo "  ── -E : show $ at line ends ──"
assert_cmd "$(printf 'hello$\nworld$\n')" cat -E "$TMPDIR"/simple.txt

echo "  ── -T : show tabs as ^I ──"
assert_cmd "with^Itab" cat -T "$TMPDIR"/tab.txt

echo "  ── -s : squeeze blank lines ──"
assert_cmd "$(printf 'a\n\nb\n')" cat -s "$TMPDIR"/squeeze.txt

echo "  ── -v : show non-printing ──"
# Tab is printed as-is with -v (no -T)
assert_cmd "$(printf 'with\ttab\n')" cat -v "$TMPDIR"/tab.txt
assert_cmd_pat '\^A\^B\^_' cat -v "$TMPDIR"/low.txt   # 0x01→^A, 0x02→^B, 0x1f→^_
assert_cmd_pat 'M-\^@'     cat -v "$TMPDIR"/high.txt   # 0x80→M-^@
assert_cmd_pat 'M-\^_'     cat -v "$TMPDIR"/high.txt   # 0x9f→M-^_
assert_cmd_pat 'M- '       cat -v "$TMPDIR"/high.txt   # 0xa0→M-<space>
assert_cmd_pat 'M-\^\?'    cat -v "$TMPDIR"/high.txt   # 0xff→M-^?

echo "  ── -A : -vET (show all) ──"
assert_cmd "with^Itab$" cat -A "$TMPDIR"/tab.txt

echo "  ── -e / -t : compound options ──"
assert_cmd "$(printf 'hello$\nworld$\n')" cat -e "$TMPDIR"/simple.txt
assert_cmd "with^Itab" cat -t "$TMPDIR"/tab.txt

echo "  ── combined -vTE ──"
assert_cmd "with^Itab$" cat -vTE "$TMPDIR"/tab.txt

echo "  ── multiple files ──"
assert_cmd "$(printf 'hello\nworld\n')" cat "$TMPDIR"/a.txt "$TMPDIR"/b.txt

echo "  ── stdin ──"
assert_cmd "stdin test" cat <<<"stdin test"
assert_cmd "dash test" cat - <<<"dash test"

echo "  ── empty file ──"
assert_cmd "" cat "$TMPDIR"/empty.txt
assert_cmd "" cat -n "$TMPDIR"/empty.txt

echo "  ── -n -s combined ──"
assert_cmd "$(printf '     1  line1\n     2  \n     3  line4\n')" cat -n -s "$TMPDIR"/mixed.txt

echo "  ── DEL character (0x7f) ──"
assert_cmd_pat '\^\?' cat -v "$TMPDIR"/del.txt

echo "  ── -vT : tab becomes ^I ──"
assert_cmd "a^Ib" cat -vT - <<< $'a\tb'

echo "  ── all-blank with -b produces no output ──"
assert_cmd "" cat -b "$TMPDIR"/blanks.txt

echo "  ── --less (non-TTY) behaves like normal cat ──"
assert_cmd "$(printf 'hello\nworld\n')" cat --less "$TMPDIR"/simple.txt

echo "  ── error: non-existent file ──"
assert_cmd_pat_stderr "No such file" cat "$TMPDIR"/nonexistent.txt

echo "  ── --head=N : show first N lines ──"
printf 'a\nb\nc\nd\ne\n' > "$TMPDIR"/head.txt
assert_cmd "$(printf 'a\nb\nc\n')" cat --head=3 "$TMPDIR"/head.txt

echo "  ── --tail=N : show last N lines ──"
assert_cmd "$(printf 'c\nd\ne\n')" cat --tail=3 "$TMPDIR"/head.txt

echo "  ── --range=N-M : show lines N-M ──"
assert_cmd "$(printf 'b\nc\nd\n')" cat --range=2-4 "$TMPDIR"/head.txt

echo "  ── --range=N- : show from N to end ──"
assert_cmd "$(printf 'd\ne\n')" cat --range=4- "$TMPDIR"/head.txt

printf 'hello world\nfoo bar\nbaz qux\nhello again\n' > "$TMPDIR"/grep.txt

echo "  ── --grep=PATTERN : filter lines ──"
assert_cmd "$(printf 'hello world\nhello again\n')" cat --grep='hello' "$TMPDIR"/grep.txt

echo "  ── --grep + --context : show context around matches ──"
assert_cmd "$(printf 'foo bar\nbaz qux\nhello again\n')" cat --grep='baz' --context=1 "$TMPDIR"/grep.txt

echo "  ── --number-format=hex : hex line numbers ──"
printf 'a\nb\n' > "$TMPDIR"/hexnum.txt
assert_cmd "$(printf '0x0001  a\n0x0002  b\n')" cat -n --number-format=hex "$TMPDIR"/hexnum.txt

echo "  ── --number-format=octal : octal line numbers ──"
assert_cmd "$(printf '000001  a\n000002  b\n')" cat -n --number-format=octal "$TMPDIR"/hexnum.txt

echo "  ── --header : show file metadata banner ──"
assert_cmd_pat 'Mode:' cat --header "$TMPDIR"/simple.txt
assert_cmd_pat 'Size:' cat --header "$TMPDIR"/simple.txt
assert_cmd_pat 'Modified:' cat --header "$TMPDIR"/simple.txt

echo "  ── --stats : show line/word/char count ──"
assert_cmd_pat 'lines.*words.*characters' cat --stats "$TMPDIR"/simple.txt

echo "  ── --head + --range combined ──"
printf '1\n2\n3\n4\n5\n6\n7\n8\n' > "$TMPDIR"/comb.txt
assert_cmd "$(printf '3\n4\n5\n')" cat --range=3-6 --head=3 "$TMPDIR"/comb.txt

echo "  ── --range + --grep combined ──"
printf 'aa\nbb\ncc\naa\nbb\ncc\n' > "$TMPDIR"/rangegrep.txt
assert_cmd "$(printf 'aa\n')" cat --range=4-6 --grep='aa' "$TMPDIR"/rangegrep.txt

echo "  ── --diff : unified diff between two files ──"
printf 'a\nb\nc\n' > "$TMPDIR"/diff1.txt
printf 'a\nd\nc\n' > "$TMPDIR"/diff2.txt
assert_cmd_pat '^---' cat --diff="$TMPDIR"/diff2.txt "$TMPDIR"/diff1.txt
assert_cmd_pat '^\+d' cat --diff="$TMPDIR"/diff2.txt "$TMPDIR"/diff1.txt

echo "  ── --blame : git blame per line ──"
BNAME="blame_test_$$"
mkdir -p "$TMPDIR"/blame_repo
cd "$TMPDIR"/blame_repo
git init -q
git config user.email "test@test.com"
git config user.name "Tester"
printf 'first line\nsecond line\n' > "$BNAME".txt
git add "$BNAME".txt
git commit -q -m "initial" --date="2026-05-30 12:00:00"
assert_cmd_pat '[0-9a-f]{7}' cat --blame "$BNAME".txt
assert_cmd_pat 'Tester' cat --blame "$BNAME".txt
cd "$TMPDIR"

echo "  ── --highlight : auto-disable ANSI when piped (not a TTY) ──"
printf '#include <stdio.h>\nint main(void) { return 0; }\n' > "$TMPDIR"/hl_test.c
OUTPUT=$("$MODBOX" cat --highlight "$TMPDIR"/hl_test.c 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\033'; then
    fail "cat --highlight (piped) — expected no ANSI codes, got them"
else
    pass "cat --highlight (piped) — ANSI codes correctly disabled"
fi

echo "  ── --highlight + --header combined ──"
assert_cmd_pat 'Mode:' cat --header --highlight "$TMPDIR"/simple.txt

echo "  ── --tui help includes --tui flag ──"
assert_cmd_pat '\-\-tui' cat --help 2>/dev/null

echo "  ── --tui single file (non-TTY) falls back to plain cat ──"
assert_cmd "$(printf 'hello\nworld\n')" cat --tui "$TMPDIR"/simple.txt

echo "  ── --tui multiple files (non-TTY) falls back to plain cat ──"
assert_cmd "$(printf 'hello\nworld\n')" cat --tui "$TMPDIR"/a.txt "$TMPDIR"/b.txt

echo "  ── --tui stdin (non-TTY) falls back to plain cat ──"
assert_cmd "stdin test" cat --tui - <<<"stdin test"

echo "  ── --tui + --number (non-TTY) falls back with line numbers ──"
assert_cmd "$(printf '     1  hello\n     2  world\n')" cat --tui -n "$TMPDIR"/simple.txt

echo "  ── --tui + --highlight (non-TTY) falls back, no ANSI ──"
OUTPUT=$("$MODBOX" cat --tui --highlight "$TMPDIR"/hl_test.c 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\033'; then
    fail "cat --tui --highlight (piped) — expected no ANSI codes, got them"
else
    pass "cat --tui --highlight (piped) — ANSI codes correctly disabled"
fi
