SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── logname ──────────────────────────────────────"

echo "  ── basic output ──"
result=$("$MODBOX" logname 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "logname (output: $result)"
else
    fail "logname — expected non-empty output"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' logname --help

echo "  ── --version ──"
assert_cmd_pat 'logname \(modbox\) 1\.0' logname --version

echo "  ── matches LOGNAME or whoami ──"
logname_result=$("$MODBOX" logname 2>/dev/null)
whoami_result=$(whoami)
if [[ "$logname_result" == "$whoami_result" ]]; then
    pass "logname matches whoami"
else
    fail "logname ($logname_result) != whoami ($whoami_result)"
fi
