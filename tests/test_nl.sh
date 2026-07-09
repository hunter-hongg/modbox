SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── nl ────────────────────────────────────────"

printf 'hello\nworld\n' > "$TMPDIR"/nl_basic
printf '\nnonempty\n\nlast\n' > "$TMPDIR"/nl_blanks
printf 'a\n\n\nb\n' > "$TMPDIR"/nl_join

echo "  ── basic nl (number all lines)"
assert_cmd "$(printf '     1\thello\n     2\tworld\n')" nl "$TMPDIR"/nl_basic

echo "  ── -b t: number nonempty only"
# File starts with a blank line, so output has leading blank line
assert_cmd "$(printf '\n     1\tnonempty\n\n     2\tlast\n')" nl -b t "$TMPDIR"/nl_blanks

echo "  ── -b n: no numbering"
assert_cmd "$(printf 'hello\nworld\n')" nl -b n "$TMPDIR"/nl_basic

echo "  ── -n rn: right-justified (default)"
assert_cmd "$(printf '     1\thello\n     2\tworld\n')" nl -n rn "$TMPDIR"/nl_basic

echo "  ── -n rz: right-justified with leading zeros"
assert_cmd "$(printf '000001\thello\n000002\tworld\n')" nl -n rz -w 6 "$TMPDIR"/nl_basic

echo "  ── -n ln: left-justified"
assert_cmd "$(printf '1     \thello\n2     \tworld\n')" nl -n ln -w 6 "$TMPDIR"/nl_basic

echo "  ── -w: custom width"
assert_cmd "$(printf '  1\thello\n  2\tworld\n')" nl -w 3 "$TMPDIR"/nl_basic

echo "  ── -s: custom separator"
assert_cmd "$(printf '     1:hello\n     2:world\n')" nl -s : "$TMPDIR"/nl_basic

echo "  ── -v: starting line number"
assert_cmd "$(printf '    10\thello\n    11\tworld\n')" nl -v 10 "$TMPDIR"/nl_basic

echo "  ── -i: line increment"
assert_cmd "$(printf '     1\thello\n     3\tworld\n')" nl -i 2 "$TMPDIR"/nl_basic

echo "  ── -l: join blank lines"
# With -l 2, pairs of blank lines count as one for numbering.
# Input: a + 2 blanks + b = 4 lines
# -b a: a→1, blank1→2, blank2→unnumbered, b→3
assert_cmd "$(printf '     1\ta\n     2\t\n\n     3\tb\n')" nl -l 2 "$TMPDIR"/nl_join

echo "  ── section delimiter: header/body/footer"
printf 'header1\n\\:\\:\\:\nbody1\n\\:\\:\nfooter1\n\\:\nextra\n' > "$TMPDIR"/nl_sections
assert_cmd "$(printf '     1\theader1\n     1\tbody1\n     1\tfooter1\n     1\textra\n')" nl "$TMPDIR"/nl_sections

echo "  ── -p: no-renumber (do not reset line numbers at sections)"
printf 'hdr\n\\:\\:\\:\nhdr2\n\\:\\:\nbody2\n' > "$TMPDIR"/nl_norenum
# With -p, line numbers continue across section boundaries (no reset)
assert_cmd "$(printf '     1\thdr\n     2\thdr2\n     3\tbody2\n')" nl -p "$TMPDIR"/nl_norenum

echo "  ── stdin"
assert_cmd "$(printf '     1\thello\n     2\tworld\n')" nl <<<"$(printf 'hello\nworld\n')"

echo "  ── help"
assert_cmd_pat 'Usage:' nl --help

echo "  ── error: nonexistent file"
assert_cmd_pat_stderr 'No such file' nl "$TMPDIR"/nl_nonexistent
