SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── seq ───────────────────────────────────────"

echo "  ── seq LAST"
assert_cmd "$(printf '1\n2\n3\n4\n5\n')" seq 5

echo "  ── seq FIRST LAST"
assert_cmd "$(printf '2\n3\n4\n5\n')" seq 2 5

echo "  ── seq FIRST INCREMENT LAST"
assert_cmd "$(printf '1\n3\n5\n7\n9\n')" seq 1 2 10

echo "  ── negative increment"
assert_cmd "$(printf '5\n4\n3\n2\n1\n')" seq 5 -1 1

echo "  ── negative first operand"
assert_cmd "$(printf -- '-3\n-2\n-1\n0\n1\n')" seq -3 1

echo "  ── floating point"
assert_cmd "$(printf '1.0\n1.5\n2.0\n2.5\n3.0\n')" seq 1 0.5 3

echo "  ── -w equal width"
assert_cmd "$(printf '08\n09\n10\n')" seq -w 8 10

echo "  ── -s separator"
assert_cmd "1,2,3,4,5" seq -s , 1 5

echo "  ── -f format"
assert_cmd "$(printf '1.00\n2.00\n3.00\n')" seq -f "%.2f" 1 3

echo "  ── empty range (first > last, positive step)"
assert_cmd "" seq 3 1

echo "  ── help"
assert_cmd_pat 'Usage:' seq --help

echo "  ── error: zero increment"
assert_cmd_pat_stderr 'increment' seq 1 0 5

echo "  ── error: invalid number"
assert_cmd_pat_stderr 'invalid' seq abc
