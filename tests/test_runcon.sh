SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── runcon ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' runcon --help

echo "  ── no command specified ──"
assert_cmd_pat 'no command specified' runcon 2>&1 || true