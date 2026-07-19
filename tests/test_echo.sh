SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── echo ─────────────────────────────────────"

echo "  ── basic ──"
assert_cmd "$(printf 'hello world')" echo hello world

echo "  ── no args (empty line) ──"
assert_cmd "" echo

echo "  ── -n (no trailing newline) ──"
result=$("$MODBOX" echo -n hi 2>/dev/null || true)
if [[ "$result" == "hi" ]]; then
    pass "echo -n hi"
else
    fail "echo -n hi — expected [hi] got [$result]"
fi

echo "  ── -e newline escape ──"
assert_cmd "$(printf 'a\nb')" echo -e "a\nb"

echo "  ── -e tab escape ──"
assert_cmd "$(printf 'a\tb')" echo -e "a\tb"

echo "  ── -e backslash escape ──"
result=$("$MODBOX" echo -e 'a\b' 2>/dev/null || true)
if [[ "$(printf 'a\x08')" == "$result" ]]; then
    pass "echo -e 'a\\b'"
else
    fail "echo -e 'a\\b' — expected [a<bs>] got [$(printf '%s' "$result" | xxd -p)]"
fi

echo "  ── -e \\c stops output (keeps prior) ──"
result=$("$MODBOX" echo -e "ab\ccd" 2>/dev/null || true)
if [[ "$result" == "ab" ]]; then
    pass "echo -e 'ab\\ccd'"
else
    fail "echo -e 'ab\\ccd' — expected [ab] got [$result]"
fi

echo "  ── -E disables escapes ──"
assert_cmd "$(printf 'a\\nb')" echo -E "a\nb"

echo "  ── no -e (literal backslash-n) ──"
assert_cmd "$(printf 'a\\nb')" echo "a\nb"

echo "  ── -e octal escape \\101 = A ──"
assert_cmd "$(printf 'A')" echo -e '\101'

echo "  ── --version ──"
assert_cmd_pat 'echo \(modbox\) 1.0' echo --version

echo "  ── --help ──"
assert_cmd_pat 'Usage:' echo --help
