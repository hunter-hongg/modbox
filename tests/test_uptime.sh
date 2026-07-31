SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── uptime ──────────────────────────────────────"

echo "  ── basic output format ──"
result=$("$MODBOX" uptime 2>/dev/null)
if [[ "$result" =~ up.*load\ average ]]; then
    pass "uptime (output: $result)"
else
    fail "uptime — expected load average output, got [$result]"
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' uptime --help

echo "  ── --pretty shows uptime ──"
result=$("$MODBOX" uptime -p 2>/dev/null)
if [[ "$result" =~ ^up ]]; then
    pass "uptime -p (output: $result)"
else
    fail "uptime -p — expected 'up ...', got [$result]"
fi