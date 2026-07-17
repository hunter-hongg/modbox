SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# GNU stat is available on this system; use it as the reference for parity.
HAVE_GNU_STAT=0
if command -v stat >/dev/null 2>&1 && LC_ALL=C stat -c '%n' "$SCRIPT_DIR/framework.sh" >/dev/null 2>&1; then
  HAVE_GNU_STAT=1
fi

echo ""
echo "── stat ────────────────────────────────────"

# Set up fixtures.
REG="$TMPDIR/reg.txt"
DIR="$TMPDIR/subdir"
LINK="$TMPDIR/link.txt"
printf 'hello world\n' > "$REG"
mkdir -p "$DIR"
ln -sf reg.txt "$LINK"

echo "  ── default verbose format (file) ──"
assert_cmd_pat 'File: ' stat "$REG"
assert_cmd_pat 'Size:' stat "$REG"
assert_cmd_pat 'regular file' stat "$REG"
assert_cmd_pat 'Access: \(' stat "$REG"
assert_cmd_pat 'Links:' stat "$REG"
assert_cmd_pat 'Modify:' stat "$REG"

echo "  ── default verbose format (directory) ──"
assert_cmd_pat 'directory' stat "$DIR"

echo "  ── symlink: name, target, type ──"
assert_cmd_pat "symbolic link" stat "$LINK"
assert_cmd_pat "' -> 'reg.txt'" stat "$LINK"

echo "  ── -L dereferences symlink to its target ──"
assert_cmd_pat 'regular file' stat -L "$LINK"
assert_cmd_pat "' -> 'reg.txt'" stat -L "$LINK"

echo "  ── -t terse format has no 'File:' header ──"
assert_cmd_not_pat 'File:' stat -t "$REG"

echo "  ── --printf: no trailing newline, interprets escapes ──"
size=$(wc -c < "$REG")
expected="${REG}"$'\t'"${size}"
assert_cmd "$expected" stat --printf='%n\t%s' "$REG"

echo "  ── --printf hex/octal escape ──"
assert_cmd "$(printf 'X\x41Y')" stat --printf='X\x41Y' "$REG"

echo "  ── multiple operands ──"
out=$("$MODBOX" stat -c '%n' "$REG" "$LINK" 2>/dev/null)
[ "$(echo "$out" | wc -l)" = "2" ] && pass "stat multiple operands -> 2 lines" || fail "stat multiple operands -> 2 lines"

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr "cannot stat 'nope.txt'" stat nope.txt
out=$("$MODBOX" stat nope.txt 2>/dev/null); [ -z "$out" ] && pass "stat nope.txt: empty stdout" || fail "stat nope.txt: empty stdout"

echo "  ── error: missing operand ──"
assert_cmd_pat_stderr "missing operand" stat

if [ "$HAVE_GNU_STAT" -eq 1 ]; then
  echo "  ── parity with GNU stat (-c file formats) ──"
  for fmt in '%n|%s' '%a %A %F' '%i %h %U %G' '%d %D %t %T %o %b %B' \
             '%w|%x' '%y %Y' '%z %Z' '%m'; do
    expected=$(LC_ALL=C stat -c "$fmt" "$REG" 2>/dev/null)
    assert_cmd "$expected" stat -c "$fmt" "$REG"
  done

  echo "  ── parity with GNU stat (-f filesystem formats) ──"
  for fmt in '%n %i %l %t %s %S %b %f %a %c %d %T'; do
    expected=$(LC_ALL=C stat -f -c "$fmt" / 2>/dev/null)
    assert_cmd "$expected" stat -f -c "$fmt" /
  done

  echo "  ── parity with GNU stat: default verbose (file type line) ──"
  assert_cmd_pat 'regular file' stat "$REG"
fi
