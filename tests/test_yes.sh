SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── yes ───────────────────────────────────────"

echo "  ── default (outputs 'y') ──"
result=$(timeout 2 "$MODBOX" yes 2>/dev/null | head -n 3 || true)
if [[ "$result" == "$(printf 'y\ny\ny')" ]]; then
    pass "yes (default)"
else
    fail "yes (default) — expected [y/y/y] got [$result]"
fi

echo "  ── single string ──"
result=$(timeout 2 "$MODBOX" yes hello 2>/dev/null | head -n 3 || true)
if [[ "$result" == "$(printf 'hello\nhello\nhello')" ]]; then
    pass "yes hello"
else
    fail "yes hello — expected [hello] got [$result]"
fi

echo "  ── multiple strings joined by space ──"
result=$(timeout 2 "$MODBOX" yes foo bar baz 2>/dev/null | head -n 2 || true)
if [[ "$result" == "$(printf 'foo bar baz\nfoo bar baz')" ]]; then
    pass "yes foo bar baz"
else
    fail "yes foo bar baz — expected [foo bar baz] got [$result]"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' yes --help

echo "  ── --version ──"
assert_cmd_pat 'yes \(modbox\) 1.0' yes --version

echo "  ── -h ──"
assert_cmd_pat 'Usage:' yes -h

echo "  ── -V ──"
assert_cmd_pat 'yes \(modbox\) 1.0' yes -V
