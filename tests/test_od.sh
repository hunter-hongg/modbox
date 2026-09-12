SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── od ─────────────────────────────────────"

TMPF="$TMPDIR/od_input"
printf 'AB\n' > "$TMPF"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' od --help

echo "  ── default octal dump ──"
assert_cmd_pat '^0000000' od "$TMPF"

echo "  ── -c prints printable chars ──"
assert_cmd_pat 'A' od -c "$TMPF"

echo "  ── -b octal bytes ──"
assert_cmd_pat '101' od -b "$TMPF"

echo "  ── -tx1 hex bytes ──"
assert_cmd_pat '41' od -tx1 "$TMPF"

echo "  ── -An suppresses address ──"
if "$MODBOX" od -An -tx1 "$TMPF" 2>/dev/null | grep -qE '^0000000'; then
    fail "-An still prints address"
else
    pass "-An suppresses address"
fi

echo "  ── -Ax hex address ──"
assert_cmd_pat '^000000 ' od -Ax -tx1 "$TMPF"

echo "  ── -td2 signed decimal 2-byte ──"
assert_cmd_pat '^0000000' od -td2 "$TMPF"

echo "  ── missing file errors ──"
assert_cmd_pat_stderr 'No such file' od /nonexistent/xyz
