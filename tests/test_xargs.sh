SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── xargs ─────────────────────────────────────"

echo " ── echo with xargs ──"
result=$(printf 'a b c\nd e f\n' | "$MODBOX" xargs echo 2>/dev/null || true)
if echo "$result" | grep -q "a"; then
  pass "xargs echo — produced output"
else
  fail "xargs echo — expected output containing 'a', got [$result]"
fi

echo " ── -n 2 limits args per invocation ──"
result=$(printf '1 2 3 4 5\n' | "$MODBOX" xargs -n 2 echo 2>/dev/null || true)
if echo "$result" | grep -q "3 4"; then
  pass "-n 2 — second group present"
else
  fail "-n 2 — got [$result]"
fi

echo " ── -0 null terminated ──"
result=$(printf 'hello\0world\n' | "$MODBOX" xargs -0 echo 2>/dev/null || true)
if echo "$result" | grep -q "hello world"; then
  pass "-0 null terminated"
else
  fail "-0 null terminated — got [$result]"
fi

echo " ── missing command error ──"
assert_cmd_pat_stderr 'missing command' xargs 2>/dev/null
echo " ── --help shows usage ──"
assert_cmd_pat 'Usage:' xargs --help
echo " ── --version ──"
assert_cmd_pat 'xargs (modbox)' xargs --version
