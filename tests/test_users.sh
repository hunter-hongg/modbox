SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── users ───────────────────────────────────────"

echo "  ── basic output matches system users ──"
sys_users=$(users 2>/dev/null || true)
result=$("$MODBOX" users 2>/dev/null)
expected=$(printf '%s' "$sys_users" | tr ' ' '\n' | sort -u | tr '\n' ' ')
actual=$(printf '%s' "$result" | tr ' ' '\n' | sort -u | tr '\n' ' ')
if [[ "$actual" == "$expected" ]]; then
    pass "users matches system output (deduplicated logins)"
else
    fail "users — expected [$expected], got [$actual]"
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' users --help