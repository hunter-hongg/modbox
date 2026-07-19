SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── env ────────────────────────────────────"

echo "  ── print env contains PATH ──"
assert_cmd_pat '^PATH=' env

echo "  ── -i clears environment ──"
assert_cmd "FOO=bar" env -i FOO=bar

echo "  ── -i with assignment ──"
assert_cmd "PATH=/bin" env -i PATH=/bin

echo "  ── run a command ──"
assert_cmd "hi" env -i echo hi

echo "  ── -u unsets variable ──"
assert_cmd_not_pat '^FOO=' env -i FOO=bar -u FOO echo

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' env --help
