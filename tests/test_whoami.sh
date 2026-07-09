SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── whoami ───────────────────────────────────"

echo "  ── print current user ──"
assert_cmd "$(whoami)" whoami

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' whoami --help
