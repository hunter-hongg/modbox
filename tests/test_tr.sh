SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tr ────────────────────────────────────────"

echo "  ── basic translate ──"
assert_cmd "$(printf 'HELLO')" tr 'a-z' 'A-Z' <<<"hello"

echo "  ── translate with range ──"
assert_cmd "$(printf 'hEllO')" tr 'a-f' 'A-F' <<<"hEllO"

echo "  ── -d : delete characters ──"
assert_cmd "$(printf '123')" tr -d 'a-z' <<<"abc123"

echo "  ── -d : delete with range ──"
assert_cmd "$(printf '123')" tr -d 'a-z' <<<"abc123"

echo "  ── -s : squeeze repeats ──"
assert_cmd "$(printf 'helo')" tr -s 'l' <<<"hello"

echo "  ── -s : squeeze spaces ──"
assert_cmd "$(printf 'a b')" tr -s ' ' <<<"a   b"

echo "  ── -s SET1 SET2 : squeeze then translate ──"
assert_cmd "$(printf 'b')" tr -s 'a' 'b' <<<"aaa"

echo "  ── -c : complement ──"
assert_cmd "$(printf 'abcZ')" tr -c 'a-z' 'Z' <<<"abc"

echo "  ── -c : complement with delete ──"
assert_cmd "$(printf 'abc')" tr -cd 'a-z\n' <<<"abc123"

echo "  ── -c : complement with squeeze ──"
printf 'a11a' | assert_cmd "$(printf 'aba')" tr -cs 'a' 'b'

echo "  ── -ds : delete + squeeze ──"
assert_cmd "$(printf 'heo')" tr -ds 'l' 'a-z' <<<"hello"

echo "  ── -t : truncate SET1 ──"
assert_cmd "$(printf '12cdef')" tr -t 'abcxyz' '12' <<<"abcdef"

echo "  ── [:lower:] → [:upper:] ──"
assert_cmd "$(printf 'HELLO')" tr '[:lower:]' '[:upper:]' <<<"hello"

echo "  ── [:digit:] delete ──"
assert_cmd "$(printf 'abc')" tr -d '[:digit:]' <<<"abc123"

echo "  ── [:space:] squeeze ──"
assert_cmd "$(printf 'a b')" tr -s '[:space:]' <<<"a    b"

echo "  ── escape sequences ──"
assert_cmd "$(printf 'a b')" tr '\t' ' ' <<<"a	b"

echo "  ── newline translation ──"
printf 'a\nb' | assert_cmd "$(printf 'a b')" tr '\n' ' '

echo "  ── octal escapes ──"
assert_cmd "$(printf 'A')" tr '\101' 'A' <<<"A"

echo "  ── help ──"
assert_cmd_pat 'Usage:' tr --help

echo "  ── stdin ──"
assert_cmd "$(printf 'ABC')" tr 'a-z' 'A-Z' <<<"abc"

echo "  ── empty input ──"
assert_cmd "" tr 'a-z' 'A-Z' < /dev/null

echo "  ── error: missing operand ──"
assert_cmd_pat_stderr 'missing operand' tr

echo "  ── error: empty SET2 ──"
assert_cmd_pat_stderr 'empty SET2' tr 'a-z' '' <<<"abc"
