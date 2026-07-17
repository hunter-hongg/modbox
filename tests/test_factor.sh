SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── factor ────────────────────────────────────"

echo "  ── small composite"
assert_cmd "12: 2 2 3" factor 12

echo "  ── prime"
assert_cmd "13: 13" factor 13

echo "  ── one"
assert_cmd "1:" factor 1

echo "  ── zero"
assert_cmd "0:" factor 0

echo "  ── large composite"
assert_cmd "1000000: 2 2 2 2 2 2 5 5 5 5 5 5" factor 1000000

echo "  ── product of two large primes"
assert_cmd "10000019: 10000019" factor 10000019

echo "  ── multiple numbers"
assert_cmd "$(printf '6: 2 3\n15: 3 5\n')" factor 6 15

echo "  ── exponents form"
assert_cmd "360: 2^3 3^2 5" factor -h 360

echo "  ── exponents form (long option)"
assert_cmd "8: 2^3" factor --exponents 8

echo "  ── stdin"
expected=$(printf '4: 2 2\n9: 3 3\n')
actual=$(printf '4 9\n' | "$MODBOX" factor 2>/dev/null)
if [[ "$actual" == "$expected" ]]; then
    pass "factor stdin"
else
    fail "factor stdin — expected [$expected] got [$actual]"
fi

echo "  ── help"
assert_cmd_pat 'Usage:' factor --help

echo "  ── error: invalid number"
assert_cmd_pat_stderr 'not a valid' factor abc

echo "  ── error: negative number"
assert_cmd_pat_stderr 'not a valid' factor -5
