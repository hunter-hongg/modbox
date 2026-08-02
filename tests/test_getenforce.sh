SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── getenforce ────────────────────────────────────"

echo "  ── basic output ──"
result=$("$MODBOX" getenforce 2>/dev/null)
if [[ "$result" == "Enforcing" ]] || [[ "$result" == "Permissive" ]] || [[ "$result" == "Disabled" ]]; then
    pass "getenforce (output: $result)"
else
    fail "getenforce — expected Enforcing/Permissive/Disabled, got [$result]"
fi

echo "  ── ground truth check ──"
if command -v /usr/sbin/getenforce >/dev/null 2>&1; then
    expected=$(/usr/sbin/getenforce)
    actual=$("$MODBOX" getenforce 2>/dev/null)
    if [[ "$actual" == "$expected" ]]; then
        pass "getenforce matches system value ($expected)"
    else
        fail "getenforce — expected [$expected], got [$actual]"
    fi
else
    echo "  SKIP — /usr/sbin/getenforce not available on this system"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' getenforce --help

echo "  ── --version ──"
assert_cmd_pat 'getenforce \(modbox\) 1\.0' getenforce --version

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'unrecognized option' getenforce --foo

echo "  ── positional argument rejected ──"
assert_cmd_pat_stderr 'unexpected argument' getenforce foo

echo "  ── stdout clean (no extra output) ──"
output=$("$MODBOX" getenforce 2>/dev/null)
line_count=$(printf '%s\n' "$output" | wc -l)
if [[ "$line_count" -eq 1 ]]; then
    pass "getenforce outputs exactly one line"
else
    fail "getenforce — expected 1 line, got $line_count"
fi
