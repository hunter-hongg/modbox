SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── df ──────────────────────────────────────"

echo " ── basic output (non-empty) ──"
result=$("$MODBOX" df . 2>/dev/null)
if [[ -n "$result" ]]; then
  pass "df (output non-empty)"
else
  fail "df — expected non-empty output"
fi

echo " ── --help ──"
assert_cmd_pat 'Usage:' df --help

echo " ── --version ──"
assert_cmd_pat 'df \(modbox\) 1\.0' df --version

echo " ── -h: human-readable ──"
assert_cmd_pat '%' df -h . 2>/dev/null

echo " ── -i: inodes ──"
assert_cmd_pat 'IUsed' df -i . 2>/dev/null

echo " ── -T: show filesystem type ──"
assert_cmd_pat 'Type' df -T . 2>/dev/null
