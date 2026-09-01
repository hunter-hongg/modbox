SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

HAVE_GNU_FILE=0
if command -v file >/dev/null 2>&1 && file "$SCRIPT_DIR/framework.sh" >/dev/null 2>&1; then
  HAVE_GNU_FILE=1
fi

echo ""
echo "── file ─────────────────────────────────────"

REG="$TMPDIR/reg.txt"
EMPTY="$TMPDIR/empty.txt"
DIR="$TMPDIR/subdir"
LINK="$TMPDIR/link.txt"
SCRIPT="$TMPDIR/script.sh"
SCRIPT_ENV="$TMPDIR/script_env.sh"
GZ="$TMPDIR/data.gz"
ELF=/usr/bin/ls

printf 'hello world\n' > "$REG"
: > "$EMPTY"
mkdir -p "$DIR"
ln -sf "$REG" "$LINK"
printf '#!/bin/bash\necho hi\n' > "$SCRIPT"
printf '#!/usr/bin/env python3\nprint("hi")\n' > "$SCRIPT_ENV"
printf 'abc' | gzip > "$GZ"
if [ ! -x "$ELF" ]; then
  ELF=/bin/echo
fi

echo "  ── ASCII text file ──"
assert_cmd_pat 'ASCII text' file "$REG"

echo "  ── empty file ──"
assert_cmd_pat 'empty' file "$EMPTY"

echo "  ── directory ──"
assert_cmd_pat 'directory' file "$DIR"

echo "  ── symlink (follow by default) ──"
assert_cmd_pat 'ASCII text' file "$LINK"

echo "  ── symlink --no-symlinks ──"
assert_cmd_pat 'symbolic link' file --no-symlinks "$LINK"

echo "  ── shell script with bash shebang ──"
assert_cmd_pat 'script executable /bin/bash' file "$SCRIPT"

echo "  ── shell script with env shebang ──"
assert_cmd_pat 'script executable /usr/bin/env python3' file "$SCRIPT_ENV"

echo "  ── gzip compressed data ──"
assert_cmd_pat 'gzip compressed' file "$GZ"

echo "  ── ELF executable ──"
assert_cmd_pat 'ELF' file "$ELF"

echo "  ── multiple operands ──"
out=$("$MODBOX" file "$REG" "$EMPTY" "$DIR" 2>/dev/null)
[ "$(echo "$out" | wc -l)" = "3" ] && pass "file multiple operands → 3 lines" || fail "file multiple operands → 3 lines"
assert_cmd_pat 'ASCII text' file "$REG" "$EMPTY" "$DIR"
assert_cmd_pat 'empty' file "$REG" "$EMPTY" "$DIR"
assert_cmd_pat 'directory' file "$REG" "$EMPTY" "$DIR"

echo "  ── nonexistent path ──"
assert_cmd_pat_stderr "No such file or directory" file "$TMPDIR/nope.txt"
out=$("$MODBOX" file "$TMPDIR/nope.txt" 2>/dev/null); [ -z "$out" ] && pass "file nope: empty stdout" || fail "file nope: empty stdout"

echo "  ── missing operand (stdin mode) ──"
assert_cmd_pat 'ASCII text' file <<< "hello"

echo "  ── stdin via - ──"
assert_cmd_pat 'ASCII text' file - <<< "hello"

echo "  ── -b brief mode ──"
assert_cmd_pat '^ASCII text$' file -b "$REG"
assert_cmd_not_pat "$REG" file -b "$REG"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' file --help
assert_cmd_pat 'Determine file type' file --help
assert_cmd_pat '\-\-brief' file --help
assert_cmd_pat '\-\-no-symlinks' file --help

if [ "$HAVE_GNU_FILE" -eq 1 ]; then
  echo "  ── parity with GNU file ──"
  # Keyword-level parity rather than byte-exact: GNU file includes host-specific
  # details (OS, arch, PIE vs executable) we intentionally omit.
  for pair in \
    "$REG:ASCII text" \
    "$EMPTY:empty" \
    "$DIR:directory" \
    "$SCRIPT:/bin/bash" \
    "$GZ:gzip compressed data" \
    "$ELF:ELF 64-bit LSB"; do
    f="${pair%%:*}"
    kw="${pair##*:}"
    assert_cmd_pat "$kw" file "$f"
  done
fi
