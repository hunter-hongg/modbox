SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tty ────────────────────────────────────"

echo "  ── non-tty stdin (tests redirect from /dev/null) ──"
assert_cmd "not a tty" tty

echo "  ── silent mode prints nothing ──"
assert_cmd "" tty -s

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' tty --help

echo "  ── --version shows version ──"
assert_cmd_pat 'tty \(modbox\)' tty --version
