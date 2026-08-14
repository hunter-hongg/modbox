SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── ip ────────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' ip --help

echo "  ── addr: shows interface names and IP addresses ──"
assert_cmd_pat 'inet' ip addr
assert_cmd_pat '127.0.0.1' ip addr
assert_cmd_pat 'mtu' ip addr

echo "  ── -4 addr: shows only IPv4 addresses ──"
assert_cmd_pat 'inet [0-9]' ip -4 addr
assert_cmd_not_pat 'inet6' ip -4 addr

echo "  ── -6 addr: shows only IPv6 addresses ──"
assert_cmd_pat 'inet6' ip -6 addr
assert_cmd_not_pat 'inet [0-9]' ip -6 addr

echo "  ── link: shows interfaces with MAC and state ──"
assert_cmd_pat 'link/loopback' ip link
assert_cmd_pat 'link/ether' ip link
assert_cmd_pat 'state' ip link

echo "  ── route: shows routing table ──"
assert_cmd_pat 'dev' ip route
assert_cmd_pat 'proto kernel' ip route

echo "  ── invalid subcommand errors ──"
assert_cmd_pat_stderr 'unknown command' ip --bad

echo "  ── no subcommand errors ──"
"$MODBOX" ip >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "ip (no args) → non-zero exit"; else fail "ip (no args) → expected non-zero, got 0"; fi
