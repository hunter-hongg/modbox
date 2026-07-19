SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── arch ──────────────────────────────────────"

echo "  ── basic output (non-empty) ──"
result=$("$MODBOX" arch 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "arch (output: $result)"
else
    fail "arch — expected non-empty output"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' arch --help

echo "  ── --version ──"
assert_cmd_pat 'arch \(modbox\) 1\.0' arch --version

echo "  ── matches uname -m ──"
arch_result=$("$MODBOX" arch 2>/dev/null)
uname_result=$(uname -m)
if [[ "$arch_result" == "$uname_result" ]]; then
    pass "arch matches uname -m"
else
    fail "arch ($arch_result) != uname -m ($uname_result)"
fi
