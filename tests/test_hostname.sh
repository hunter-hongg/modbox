SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── hostname ────────────────────────────────────"

SYS_HOSTNAME=$(hostname)
SYS_SHORT=${SYS_HOSTNAME%%.*}

echo "  ── basic output ──"
assert_cmd "$SYS_SHORT" hostname

echo "  ── -s short name ──"
assert_cmd "$SYS_SHORT" hostname -s

echo "  ── -f fqdn ──"
result=$("$MODBOX" hostname -f 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "hostname -f (output: $result)"
else
    fail "hostname -f — expected non-empty output"
fi

echo "  ── -i addresses ──"
result=$("$MODBOX" hostname -i 2>/dev/null)
if [[ "$result" =~ ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+|:) ]] || [[ -z "$result" ]]; then
    pass "hostname -i (output: $result)"
else
    fail "hostname -i — expected IP addresses, got [$result]"
fi

echo "  ── -I all addresses ──"
result=$("$MODBOX" hostname -I 2>/dev/null)
if [[ "$result" =~ ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+|:) ]] || [[ -z "$result" ]]; then
    pass "hostname -I"
else
    fail "hostname -I — expected IP addresses, got [$result]"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage: hostname' hostname --help

echo "  ── --version ──"
assert_cmd_pat 'hostname \(modbox\) 1\.0' hostname --version
