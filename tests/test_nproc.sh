SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── nproc ──────────────────────────────────────"

echo "  ── basic output (positive integer) ──"
result=$("$MODBOX" nproc 2>/dev/null)
if [[ "$result" =~ ^[0-9]+$ ]] && [[ "$result" -gt 0 ]]; then
    pass "nproc (output: $result)"
else
    fail "nproc — expected positive integer, got [$result]"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' nproc --help

echo "  ── --version ──"
assert_cmd_pat 'nproc \(modbox\) 1\.0' nproc --version
