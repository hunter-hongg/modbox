SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── chcon ────────────────────────────────────"

echo "  ── help ──"
assert_cmd_pat 'Usage:' chcon --help

echo "  ── error: no arguments ──"
assert_cmd_pat_stderr 'missing' chcon

echo "  ── error: file not found ──"
assert_cmd_pat_stderr 'failed' chcon -u system_u "$TMPDIR"/chcon_nonexistent
