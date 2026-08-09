SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lsblk ──────────────────────────────────────"

echo "  ── basic output (non-empty) ──"
result=$("$MODBOX" lsblk 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "lsblk (output non-empty)"
else
    fail "lsblk — expected non-empty output"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' lsblk --help

echo "  ── --version ──"
assert_cmd_pat 'lsblk \(modbox\) 1\.0' lsblk --version

echo "  ── -n: no headings ──"
assert_cmd_not_pat 'NAME' lsblk -n 2>/dev/null

echo "  ── --json ──"
json_result=$("$MODBOX" lsblk --json 2>/dev/null)
if [[ "$json_result" == "["* ]] && [[ "$json_result" == *"]" ]]; then
    pass "lsblk --json (valid JSON structure)"
else
    fail "lsblk --json — expected JSON array"
fi

echo "  ── -b: bytes ──"
bytes_result=$("$MODBOX" lsblk -b 2>/dev/null)
# Should contain numeric values (sizes in bytes)
if [[ -n "$bytes_result" ]]; then
    pass "lsblk -b (output non-empty)"
else
    fail "lsblk -b — expected non-empty output"
fi

echo "  ── -a: all devices ──"
all_result=$("$MODBOX" lsblk -a 2>/dev/null)
if [[ -n "$all_result" ]]; then
    pass "lsblk -a (output non-empty)"
else
    fail "lsblk -a — expected non-empty output"
fi
