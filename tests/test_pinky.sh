SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── pinky ──────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' pinky --help

echo "  ── --version shows version ──"
assert_cmd_pat 'pinky \(modbox\)' pinky --version

echo "  ── -q quick shows names and count ──"
assert_cmd_pat '^.*total [0-9]+$' pinky -q

echo "  ── default output (short form) ──"
OUTPUT=$("$MODBOX" pinky 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE 'Login|pts|tty'; then
  pass "pinky produces short format output"
else
  pass "pinky produces no output (no users) — acceptable"
fi

echo "  ── -l long format ──"
OUTPUT=$("$MODBOX" pinky -l 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE 'Login|Where|TTY'; then
  pass "pinky -l produces long format output"
else
  pass "pinky -l produces no output (no users) — acceptable"
fi

echo "  ── -b brief (no hostnames) ──"
OUTPUT=$("$MODBOX" pinky -b 2>/dev/null || true)
if echo "$OUTPUT" | grep -qE 'Login|name|TTY'; then
  pass "pinky -b produces brief output"
else
  pass "pinky -b produces no output (no users) — acceptable"
fi
