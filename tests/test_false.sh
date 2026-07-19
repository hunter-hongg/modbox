SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── false ─────────────────────────────────────"

echo "  ── default (exit 1) ──"
if ! "$MODBOX" false 2>/dev/null; then
    pass "false (exit 1)"
else
    fail "false (exit 1) — expected exit 1"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' false --help

echo "  ── --version ──"
assert_cmd_pat 'false \(modbox\) 1\.0' false --version

echo "  ── ignores arguments ──"
if ! "$MODBOX" false foo bar baz 2>/dev/null; then
    pass "false foo bar baz"
else
    fail "false foo bar baz — expected exit 1"
fi
