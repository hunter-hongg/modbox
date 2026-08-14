SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── ss ────────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' ss --help

echo "  ── -tln: shows TCP listening sockets ──"
assert_cmd_pat 'LISTEN' ss -tln
assert_cmd_pat 'Local Address:Port' ss -tln

echo "  ── -tan: shows all TCP sockets ──"
assert_cmd_pat 'State' ss -tan
assert_cmd_pat 'Local Address:Port' ss -tan

echo "  ── -uln: shows UDP listening sockets ──"
assert_cmd_pat 'UNCONN' ss -uln

echo "  ── -4 -tln: filters to IPv4 only ──"
assert_cmd_pat '0\.0\.0\.0' ss -4 -tln || assert_cmd_pat '127\.0\.0\.' ss -4 -tln

echo "  ── invalid option errors ──"
assert_cmd_pat_stderr 'unknown option' ss --bad
