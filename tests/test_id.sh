SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── id ─────────────────────────────────────"

echo "  ── -u prints effective uid ──"
assert_cmd "$MY_UID" id -u

echo "  ── -un prints user name ──"
assert_cmd "$(id -un)" id -un

echo "  ── -g prints effective gid ──"
assert_cmd "$MY_GID" id -g

echo "  ── -gn prints group name ──"
assert_cmd "$(id -gn)" id -gn

echo "  ── id shows uid=gid ──"
assert_cmd_pat "uid=$MY_UID" id

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' id --help
