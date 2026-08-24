SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── mpstat ───────────────────────────────────────"

echo "  ── basic output (non-empty, has CPU stats) ──"
result=$("$MODBOX" mpstat 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "mpstat (output non-empty)"
else
    fail "mpstat — expected non-empty output"
fi

echo "  ── output contains CPU utilization fields ──"
assert_cmd_pat '%usr|%sys|%idle|CPU' mpstat

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' mpstat --help

echo "  ── --version shows version ──"
assert_cmd_pat 'mpstat \(modbox\) 1\.0' mpstat --version

echo "  ── --all shows per-CPU lines ──"
result=$("$MODBOX" mpstat --all 2>/dev/null)
if echo "$result" | grep -qE 'cpu[0-9]'; then
    pass "mpstat --all (has per-CPU lines)"
else
    fail "mpstat --all — expected per-CPU lines (cpu0, cpu1, ...), got: $result"
fi

echo "  ── --json produces JSON object ──"
json_result=$("$MODBOX" mpstat --json 2>/dev/null)
if [[ "$json_result" == "{"* ]] && [[ "$json_result" == *"}" ]]; then
    pass "mpstat --json (valid JSON structure)"
else
    fail "mpstat --json — expected JSON object, got: $json_result"
fi

echo "  ── invalid option errors ──"
assert_cmd_pat_stderr 'invalid option' mpstat --bogus
