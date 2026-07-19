SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── true ──────────────────────────────────────"

echo "  ── default (exit 0) ──"
if "$MODBOX" true 2>/dev/null; then
    pass "true (exit 0)"
else
    fail "true (exit 0) — expected exit 0"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' true --help

echo "  ── --version ──"
assert_cmd_pat 'true \(modbox\) 1\.0' true --version

echo "  ── ignores arguments ──"
if "$MODBOX" true foo bar baz 2>/dev/null; then
    pass "true foo bar baz"
else
    fail "true foo bar baz — expected exit 0"
fi
