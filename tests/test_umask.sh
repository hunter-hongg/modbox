SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── umask ────────────────────────────────────"

echo "  ── default output (octal) ──"
assert_cmd_pat '^[0-7]{4}$' umask

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' umask --help

echo "  ── --version shows version ──"
assert_cmd_pat 'umask \(modbox\)' umask --version

echo "  ── -S symbolic output ──"
assert_cmd_pat 'u=' umask -S

echo "  ── -p printable output ──"
assert_cmd_pat '^umask [0-7]{4}$' umask -p

echo "  ── set mask via octal ──"
assert_cmd "" umask 0000
# Restore default
umask 0022 2>/dev/null || true

echo "  ── invalid mask errors ──"
assert_cmd_pat_stderr 'invalid' umask xyz

echo "  ── -S with mask set shows symbolic ──"
# Run umask -S 0000 to get symbolic output, then restore
OUTPUT=$(MODBOX=$MODBOX "$MODBOX" umask -S 0000 2>/dev/null || true)
if echo "$OUTPUT" | grep -q 'u='; then
  pass "umask -S 0000 → symbolic output"
else
  fail "umask -S 0000 — expected symbolic output"
fi
umask 0022 2>/dev/null || true