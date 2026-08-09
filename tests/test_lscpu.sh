SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lscpu ──────────────────────────────────────"

echo "  ── basic output (non-empty) ──"
result=$("$MODBOX" lscpu 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "lscpu (output non-empty)"
else
    fail "lscpu — expected non-empty output"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' lscpu --help

echo "  ── --version ──"
assert_cmd_pat 'lscpu \(modbox\) 1\.0' lscpu --version

echo "  ── --json ──"
json_result=$("$MODBOX" lscpu --json 2>/dev/null)
if [[ "$json_result" == "{"* ]] && [[ "$json_result" == *"}" ]]; then
    pass "lscpu --json (valid JSON structure)"
else
    fail "lscpu --json — expected JSON object"
fi

echo "  ── -e: extended ──"
ext_result=$("$MODBOX" lscpu -e 2>/dev/null)
if [[ -n "$ext_result" ]]; then
    pass "lscpu -e (output non-empty)"
else
    fail "lscpu -e — expected non-empty output"
fi

echo "  ── output contains Architecture ──"
assert_cmd_pat 'Architecture:' lscpu
