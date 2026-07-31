SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── users ───────────────────────────────────────"

echo "  ── basic output matches system users ──"
result=$("$MODBOX" users 2>/dev/null)
sys_users=$(users 2>/dev/null || true)
if [[ "$result" == "$sys_users" ]]; then
    pass "users matches system output"
else
    fail "users — expected [$sys_users], got [$result]"
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' users --help