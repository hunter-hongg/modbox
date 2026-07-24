SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── hostid ──────────────────────────────────────"

echo "  ── basic output ──"
result=$("$MODBOX" hostid 2>/dev/null)
if [[ -n "$result" ]] && [[ "$result" =~ ^[0-9a-fA-F]+$ ]]; then
    pass "hostid (output: $result)"
else
    fail "hostid — expected hex output, got [$result]"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' hostid --help

echo "  ── --version ──"
assert_cmd_pat 'hostid \(modbox\) 1\.0' hostid --version
