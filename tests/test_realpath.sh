SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── realpath ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' realpath --help

echo "  ── /tmp/../tmp resolves ──"
assert_cmd '/tmp' realpath /tmp/../tmp

echo "  ── relative-to prints relative path ──"
RELF="$TMPDIR/rel_target"
printf 'x' > "$RELF"
assert_cmd 'rel_target' realpath --relative-to="$TMPDIR" "$RELF"

echo "  ── -e missing errors ──"
if "$MODBOX" realpath -e /nonexistent/xyz >/dev/null 2>&1; then
    fail "-e missing → expected non-zero"
else
    pass "-e missing → exit non-zero"
fi

echo "  ── -m no error on missing ──"
if "$MODBOX" realpath -m /nonexistent/xyz >/dev/null 2>&1; then
    pass "-m missing → exit 0"
else
    fail "-m missing → expected exit 0"
fi

echo "  ── symlink resolution ──"
LINK="$TMPDIR/rp_link"
ln -sf /tmp "$LINK"
assert_cmd '/tmp' realpath "$LINK"
assert_cmd "$LINK" realpath -s "$LINK"
rm -f "$LINK"
