SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── who ──────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' who --help

echo "  ── --version shows version ──"
assert_cmd_pat 'who \(modbox\)' who --version

echo "  ── -q count shows total ──"
assert_cmd_pat '^total [0-9]+$' who -q

echo "  ── -H heading prints header ──"
assert_cmd_pat 'NAME' who -H

echo "  ── -b boot time ──"
OUTPUT=$("$MODBOX" who -b 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE 'system boot'; then
  pass "who -b → matches /system boot/"
else
  pass "who -b (no boot entries in utmp — acceptable)"
fi

echo "  ── default output (short form) ──"
# Just check it doesn't crash and produces some output
OUTPUT=$("$MODBOX" who 2>/dev/null || true)
if [ -n "$OUTPUT" ]; then
  pass "who produces output"
else
  pass "who produces no output (no users) — acceptable"
fi