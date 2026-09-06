SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── column ──────────────────────────────────────"

# Helper: run column on stdin via a pipe and compare to an exact expected string.
#   expect_equal DESCRIPTION EXPECTED INPUT... (INPUT is piped through printf %b)
expect_equal() {
  local desc="$1"; local expected="$2"; local input="$3"
  local actual
  actual=$(printf '%b' "$input" | "$MODBOX" column "${@:4}" 2>/dev/null || true)
  if [[ "$actual" == "$expected" ]]; then
    pass "$desc"
  else
    fail "$desc — expected [$(printf '%s' "$expected" | tr '\n' '|')] got [$(printf '%s' "$actual" | tr '\n' '|')]"
  fi
}

# ── Fill mode (default) ────────────────────────────────────────────────────
echo " ── fill: everything fits on one row ──"
expect_equal "fill single row" $'a b c' 'a\nb\nc\n' -c 80

echo " ── fill: multi-column, filled down (seq 8 -c 10) ──"
result=$(seq 8 | "$MODBOX" column -c 10 2>/dev/null || true)
if [[ "$result" == $'1 3 5 7\n2 4 6 8' ]]; then
  pass "fill multi-column layout"
else
  fail "fill multi-column layout — got [$(printf '%s' "$result" | tr '\n' '|')]"
fi

echo " ── fill: -c narrows the layout (seq 8 -c 4) ──"
result4=$(seq 8 | "$MODBOX" column -c 4 2>/dev/null || true)
if [[ "$result4" == $'1 5\n2 6\n3 7\n4 8' ]]; then
  pass "fill -c 4 two-column layout"
else
  fail "fill -c 4 — got [$(printf '%s' "$result4" | tr '\n' '|')]"
fi

echo " ── fill: empty input produces no output ──"
empty_out=$(printf '' | "$MODBOX" column 2>/dev/null || true)
if [[ -z "$empty_out" ]]; then
  pass "fill empty input"
else
  fail "fill empty input — got [$empty_out]"
fi

# ── Table mode core (-t, -s) ───────────────────────────────────────────────
echo " ── table: basic alignment ──"
expect_equal "table basic" $'a b\nc dd' 'a\tb\nc\tdd\n' -t

echo " ── table: -s comma separator (CSV) ──"
expect_equal "table -s comma" $'a   b  cc\nddd ee fff' 'a,b,cc\nddd,ee,fff\n' -s , -t

echo " ── table: -s non-space separator ──"
expect_equal "table -s \$" $'x    y\nlong z' 'x$y\nlong$z\n' -s '$' -t

echo " ── table: default whitespace (spaces and tabs) ──"
expect_equal "table default ws" $'foo bar baz\nx   y   z' 'foo  bar\tbaz\nx\ty\tz\n' -t

echo " ── table: ragged rows padded, no trailing spaces ──"
expect_equal "table ragged" $'a b c\nd' 'a,b,c\nd\n' -s , -t

echo " ── table: empty input produces no output ──"
table_empty=$(printf '' | "$MODBOX" column -t 2>/dev/null || true)
if [[ -z "$table_empty" ]]; then
  pass "table empty input"
else
  fail "table empty input — got [$table_empty]"
fi

# ── Table alignment & formatting ───────────────────────────────────────────
echo " ── table: -r right-align all ──"
expect_equal "table -r" $'  a  b\nccc dd' 'a,b\nccc,dd\n' -s , -t -r

echo " ── table: -R right-align all (alias) ──"
expect_equal "table -R" $'  a  b\nccc dd' 'a,b\nccc,dd\n' -s , -t -R

echo " ── table: -C right-align rightmost only ──"
expect_equal "table -C" $'a    b\nccc dd' 'a,b\nccc,dd\n' -s , -t -C

echo " ── table: -d divider between columns ──"
expect_equal "table -d" $'a  b\nc  d' 'a,b\nc,d\n' -s , -t -d

echo " ── table: -L indent every line ──"
expect_equal "table -L" $'  a b\n  c d' 'a,b\nc,d\n' -s , -t -L "  "

echo " ── table: -N override column names ──"
expect_equal "table -N" $'x y\nc d' 'a,b\nc,d\n' -s , -t -N "x,y"

echo " ── table: -N with '-' keeps original ──"
expect_equal "table -N keep" $'a y\nc d' 'a,b\nc,d\n' -s , -t -N "-,y"

echo " ── table: -o writes to a file, stdout empty ──"
outfile="$TMPDIR/column_out.txt"
out=$(printf 'a,b\nc,dd\n' | "$MODBOX" column -s , -t -o "$outfile" 2>/dev/null || true)
filecontent=$(cat "$outfile" 2>/dev/null || true)
if [[ -z "$out" && "$filecontent" == $'a b\nc dd' ]]; then
  pass "table -o file output"
else
  fail "table -o file output — stdout [$out] file [$filecontent]"
fi
rm -f "$outfile"

echo " ── table: combined -t -r -d ──"
expect_equal "table -r -d" $'  a   b\nccc  dd' 'a,b\nccc,dd\n' -s , -t -r -d

# ── Advanced multi-line options ────────────────────────────────────────────
echo " ── table: -e entry separator (single line) ──"
expect_equal "table -e" $'x    y\nlong z' 'x$y\nlong$z\n' -e '$' -t

echo " ── table: -l groups lines into multi-line records ──"
expect_equal "table -l 2 -e" $'col1a col2a\ncol1b col2b' 'col1a$col2a\ncol1b$col2b\n' -l 2 -e '$' -t

echo " ── table: -H repeats the header periodically ──"
headcount=$( { echo "H1,H2"; for i in $(seq 1 30); do echo "r$i,c$i"; done; } | "$MODBOX" column -s , -t -H 2>/dev/null | grep -c "H1" || true)
if [[ "$headcount" -ge 2 ]]; then
  pass "table -H repeats header (saw $headcount occurrences)"
else
  fail "table -H — expected header repeated, saw $headcount occurrences"
fi

echo " ── table: -a accepted (full output) ──"
expect_equal "table -a" $'a b\nc dd' 'a\tb\nc\tdd\n' -t -a

# ── I/O & files ────────────────────────────────────────────────────────────
echo " ── reads a named file ──"
tmpin="$TMPDIR/column_in.txt"
printf 'a,b\nc,dd\n' > "$tmpin"
assert_cmd "a b
c dd" column -s , -t "$tmpin"
rm -f "$tmpin"

echo " ── multiple files concatenated ──"
f1="$TMPDIR/column_f1.txt"; f2="$TMPDIR/column_f2.txt"
printf 'a,b\n' > "$f1"; printf 'c,dd\n' > "$f2"
assert_cmd "a b
c dd" column -s , -t "$f1" "$f2"
rm -f "$f1" "$f2"

# ── Help / version / errors ────────────────────────────────────────────────
echo " ── --help shows usage ──"
assert_cmd_pat 'Usage:' column --help

echo " ── --version ──"
assert_cmd_pat 'column \(modbox\)' column --version

echo " ── --help lists table options ──"
assert_cmd_pat 'table-column-names' column --help

echo " ── missing file prints error to stderr ──"
assert_cmd_pat_stderr 'column:' column -t "$TMPDIR/does_not_exist_column"

echo " ── unrecognized option errors ──"
assert_cmd_pat_stderr 'unrecognized option' column --not-a-real-option
