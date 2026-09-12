SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── shred ─────────────────────────────────────"

TMPF="$TMPDIR/shred_input"
printf 'secret data\n' > "$TMPF"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' shred --help

echo "  ── shreds and removes file ──"
"$MODBOX" shred -u -n 1 "$TMPF" >/dev/null 2>&1
if [[ ! -f "$TMPF" ]]; then
    pass "shred -u removes file"
else
    fail "shred -u did not remove file"
fi

echo "  ── overwrites without removing by default ──"
printf 'secret data\n' > "$TMPF"
"$MODBOX" shred -n 1 "$TMPF" >/dev/null 2>&1
if [[ -f "$TMPF" ]]; then
    pass "shred keeps file without -u"
else
    fail "shred removed file without -u"
fi
# After shredding, contents should differ from original
if grep -q 'secret data' "$TMPF" 2>/dev/null; then
    fail "shred did not overwrite contents"
else
    pass "shred overwrites contents"
fi

echo "  ── missing file errors ──"
assert_cmd_pat_stderr 'No such file' shred /nonexistent/xyz
