SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── iostat ───────────────────────────────────────"

echo "  ── basic output (non-empty, has header) ──"
result=$("$MODBOX" iostat 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "iostat (output non-empty)"
else
    fail "iostat — expected non-empty output"
fi

echo "  ── output contains Device or tps header ──"
assert_cmd_pat 'Device|tps' iostat

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' iostat --help

echo "  ── --version shows version ──"
assert_cmd_pat 'iostat \(modbox\) 1\.0' iostat --version

echo "  ── --json produces JSON object ──"
json_result=$("$MODBOX" iostat --json 2>/dev/null)
if [[ "$json_result" == "{"* ]] && [[ "$json_result" == *"}" ]]; then
    pass "iostat --json (valid JSON structure)"
else
    fail "iostat --json — expected JSON object, got: $json_result"
fi

echo "  ── invalid option errors ──"
assert_cmd_pat_stderr 'invalid option' iostat --bogus

echo "  ── -S K scales to KB ──"
result=$("$MODBOX" iostat -S K 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "iostat -S K (output non-empty)"
else
    fail "iostat -S K — expected non-empty output"
fi
