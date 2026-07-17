SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── expr ────────────────────────────────────"

echo "  ── arithmetic ──"
assert_cmd "2" expr 1 + 1
assert_cmd "7" expr 1 + 2 '*' 3
assert_cmd "5" expr 10 - 5
assert_cmd "3" expr 7 / 2
assert_cmd "-3" expr -7 / 2
assert_cmd "-1" expr -7 % 2
assert_cmd "1000000000000000000000000" expr 1000000000000 '*' 1000000000000

echo "  ── precedence ──"
assert_cmd "11" expr 1 + 2 '*' 3 + 4
assert_cmd "14" expr 2 + 3 '*' 4
assert_cmd "20" expr '(' 2 + 3 ')' '*' 4
assert_cmd "1" expr 3 '<' 5 + 1
assert_cmd "2" expr 1 + 2 : '.*'
assert_cmd "5" expr 5 '*' 2 : '.*'

echo "  ── comparison (integer) ──"
assert_cmd "1" expr 2 '<' 3 '|' 5 '<' 20
assert_cmd "0" expr 10 '<' 2
assert_cmd "1" expr 5 '=' 05
assert_cmd "0" expr 3 '!=' 3

echo "  ── comparison (string) ──"
assert_cmd "1" expr aaa '=' aaa
assert_cmd "0" expr aaa '=' aab
assert_cmd "1" expr a '<' b

echo "  ── '|' and '&' ──"
assert_cmd "abc" expr '' '|' abc
assert_cmd "foo" expr foo '|' bar
assert_cmd "bar" expr 0 '|' bar
assert_cmd "3" expr 3 '&' 5
assert_cmd "0" expr 0 '&' 5
assert_cmd "0" expr '' '&' 5

echo "  ── regex : and match ──"
assert_cmd "b" expr abc : 'a\(.\)c'
assert_cmd "3" expr abc : 'abc'
assert_cmd "0" expr xabc : abc
assert_cmd "0" expr abc : xyz
assert_cmd "b" expr match abc 'a\(.\)c'
assert_cmd "abc" expr abc : '\(abc\)'

echo "  ── substr / index / length ──"
assert_cmd "3" expr length abc
assert_cmd "bc" expr substr abc 2 2
assert_cmd "" expr substr abc 2 0
assert_cmd "" expr substr abc 100 2
assert_cmd "3" expr index abc c
assert_cmd "0" expr index abc z

echo "  ── parentheses ──"
assert_cmd "10" expr '(' 1 + 2 ')' '*' '(' 3 - 1 ')' + 4

echo "  ── exit codes ──"
chk() {
    local expected="$1"; shift
    "$MODBOX" expr "$@" >/dev/null 2>&1
    local rc=$?
    if [[ $rc -eq $expected ]]; then
        pass "expr $* → exit $expected"
    else
        fail "expr $* → expected exit $expected, got $rc"
    fi
}
chk 0 1
chk 1 0
chk 1 ''
chk 0 abc
chk 0 5
chk 1 4 - 4
chk 0 2 '*' 2 '>' 3
chk 2 1 + a
chk 2 5 / 0
chk 2 5 -3
chk 2 1 +
chk 2 length
chk 2

echo "  ── --help and --version ──"
assert_cmd_pat 'Usage:' expr --help
assert_cmd_pat 'modbox' expr --version
