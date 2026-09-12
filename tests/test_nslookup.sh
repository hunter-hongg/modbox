SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── nslookup ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' nslookup --help

echo "  ── no domain errors ──"
assert_cmd_pat_stderr 'missing option' nslookup

echo "  ── unknown record type errors ──"
assert_cmd_pat_stderr 'unknown record type' nslookup -t BOGUS example.com

echo "  ── A record query (network) ──"
out=$("$MODBOX" nslookup example.com 2>/dev/null || true)
if printf '%s' "$out" | grep -qE 'Server:|Address'; then
    pass "nslookup A query returns output"
else
    echo "  SKIP  nslookup A query (no network)"
fi
