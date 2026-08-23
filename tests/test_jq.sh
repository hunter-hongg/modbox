SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── jq ───────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' jq --help

echo "  ── --version shows modbox ──"
assert_cmd_pat 'modbox' jq --version

echo "  ── no arguments is usage error (exit 2) ──"
"$MODBOX" jq >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "jq (no args) → exit 2"; else fail "jq (no args) → exit $rc, expected 2"; fi

echo "  ── basic field extraction ──"
echo '{"name":"Alice"}' | "$MODBOX" jq '.name' > "$TMPDIR/out.txt"
if grep -q '"Alice"' "$TMPDIR/out.txt"; then pass "field extraction"; else fail "field extraction"; fi

echo "  ── raw output ──"
echo '{"name":"Bob"}' | "$MODBOX" jq -r '.name' > "$TMPDIR/out.txt"
if grep -q '^Bob$' "$TMPDIR/out.txt"; then pass "raw output"; else fail "raw output"; fi

echo "  ── compact output ──"
echo '{"a":1}' | "$MODBOX" jq -c '.a' > "$TMPDIR/out.txt"
if grep -q '^1$' "$TMPDIR/out.txt"; then pass "compact output"; else fail "compact output"; fi

echo "  ── length function ──"
echo '[1,2,3]' | "$MODBOX" jq 'length' > "$TMPDIR/out.txt"
if grep -q '3' "$TMPDIR/out.txt"; then pass "length"; else fail "length"; fi

echo "  ── stdin handling ──"
echo '{"a":1}' | "$MODBOX" jq '.a' > "$TMPDIR/out.txt"
if grep -q '1' "$TMPDIR/out.txt"; then pass "stdin"; else fail "stdin"; fi
