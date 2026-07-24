SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── groups ──────────────────────────────────────"

echo " ── --help ──"
assert_cmd_pat 'Usage:' groups --help

echo " ── --version ──"
assert_cmd_pat 'groups \(modbox\) 1\.0' groups --version

echo " ── no args (current user) ──"
result=$("$MODBOX" groups 2>/dev/null)
if [[ -n "$result" ]]; then
  pass "groups (output non-empty)"
else
  fail "groups — expected non-empty output"
fi

echo " ── with username ──"
result=$("$MODBOX" groups root 2>/dev/null)
if [[ -n "$result" ]]; then
  pass "groups root (output non-empty)"
else
  fail "groups root — expected non-empty output"
fi
