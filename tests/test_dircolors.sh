SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── dircolors ──────────────────────────────────────"

echo " ── --help ──"
assert_cmd_pat 'Usage:' dircolors --help

echo " ── --version ──"
assert_cmd_pat 'dircolors \(modbox\) 1\.0' dircolors --version

echo " ── -p: print database ──"
assert_cmd_pat 'RESET' dircolors -p

echo " ── default (bourne shell, no args) ──"
result=$("$MODBOX" dircolors 2>/dev/null)
if echo "$result" | grep -q 'LS_COLORS'; then
  pass "dircolors default outputs LS_COLORS"
else
  fail "dircolors default — expected LS_COLORS in output"
fi

echo " ── -b: bourne shell ──"
assert_cmd_pat 'export LS_COLORS' dircolors -b

echo " ── -c: c shell ──"
assert_cmd_pat 'setenv LS_COLORS' dircolors -c
