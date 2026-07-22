SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── fold ──────────────────────────────────────"

echo " ── basic wrap at default width 80 (short line unchanged) ──"
result=$(printf "hello world\n" | "$MODBOX" fold 2>/dev/null || true)
if [[ "$result" == "hello world" ]]; then
  pass "short line unchanged"
else
  fail "short line unchanged — got [$result]"
fi

echo " ── wrap long line ──"
input=$(printf '%*s' 100 '' | tr ' ' 'x')
result=$(printf '%s\n' "$input" | "$MODBOX" fold -w 10 2>/dev/null || true)
linecount=$(echo "$result" | wc -l)
if [[ "$linecount" -eq 10 ]]; then
  pass "wrap at width 10 — got 10 lines"
else
  fail "wrap at width 10 — expected 10 lines, got $linecount"
fi

echo " ── -s break at spaces ──"
input2="The quick brown fox jumps over the lazy dog"
result2=$(printf '%s\n' "$input2" | "$MODBOX" fold -w 15 -s 2>/dev/null || true)
if echo "$result2" | grep -qE "The quick"; then
  pass "break at spaces — first line ok"
else
  fail "break at spaces — got [$result2]"
fi

echo " ── --help shows usage ──"
assert_cmd_pat 'Usage:' fold --help
echo " ── --version ──"
assert_cmd_pat 'fold (modbox)' fold --version
