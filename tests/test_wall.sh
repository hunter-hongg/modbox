SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── wall ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' wall --help

echo "  ── --version shows version ──"
assert_cmd_pat 'wall \(modbox\)' wall --version

echo "  ── -n nobanner requires root ──"
# Only test if not root
if [ "$(id -u)" != "0" ]; then
  assert_cmd_pat_stderr 'cannot use --nobanner' wall -n test
else
  pass "wall -n test (skipped as root)"
fi

echo "  ── -g with bad group errors ──"
assert_cmd_pat_stderr 'unknown group' wall -g nonexistent_group test