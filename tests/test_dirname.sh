SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── dirname ───────────────────────────────────"

echo "  ── strip last component ──"
assert_cmd '/usr' dirname /usr/bin

echo "  ── single component (no slash) ──"
assert_cmd '.' dirname foo

echo "  ── root ──"
assert_cmd '/' dirname /

echo "  ── trailing slash ──"
assert_cmd '/' dirname /usr/

echo "  ── deep path ──"
assert_cmd '/a/b' dirname /a/b/c

echo "  ── relative path ──"
assert_cmd 'a' dirname a/b

echo "  ── dot ──"
assert_cmd '.' dirname .

echo "  ── multiple args ──"
result=$("$MODBOX" dirname /a/b /c/d 2>/dev/null)
expected=$(printf '/a\n/c')
if [[ "$result" == "$expected" ]]; then
    pass "dirname /a/b /c/d"
else
    fail "dirname /a/b /c/d — expected [$(echo "$expected" | head -c 80)] got [$(echo "$result" | head -c 80)]"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' dirname --help

echo "  ── --version ──"
assert_cmd_pat 'dirname \(modbox\) 1\.0' dirname --version
