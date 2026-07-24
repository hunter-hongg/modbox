SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── chroot ───────────────────────────────────"

echo "  ── help ──"
assert_cmd_pat 'Usage:' chroot --help

echo "  ── error: missing NEWROOT ──"
assert_cmd_pat_stderr 'missing' chroot
