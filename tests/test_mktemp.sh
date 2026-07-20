SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── mktemp ────────────────────────────────────"

echo " ── default temp file ──"
result=$("$MODBOX" mktemp 2>/dev/null || true)
test -f "$result" && pass "mktemp created temp file [$result]" || fail "mktemp — no file created"

echo " ── -t prefix mode ──"
result=$("$MODBOX" mktemp -t modboxtest 2>/dev/null || true)
test -f "$result" && echo "$result" | grep -qE "^/tmp/modboxtest" && pass "mktemp -t created prefixed file [$result]" || fail "mktemp -t — expected /tmp/modboxtest*"

echo " ── --help ──"
assert_cmd_pat 'Usage:' "$MODBOX" mktemp --help
