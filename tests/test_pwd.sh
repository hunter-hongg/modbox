SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── pwd ────────────────────────────────────"

echo "  ── print current directory ──"
assert_cmd "$PWD" pwd

echo "  ── -P prints physical path ──"
assert_cmd "$(pwd -P)" pwd -P

echo "  ── -L prints logical path ──"
assert_cmd "$PWD" pwd -L

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' pwd --help

echo "  ── --version shows version ──"
assert_cmd_pat 'pwd \(modbox\)' pwd --version
