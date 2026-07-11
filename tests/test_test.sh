SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

WORK="$TMPDIR/test_cmd"
mkdir -p "$WORK"
touch "$WORK/file"
printf 'x' > "$WORK/file"
mkdir "$WORK/dir"
ln -sf "$WORK/file" "$WORK/link"

echo ""
echo "── test ────────────────────────────────────"

chk() {
    local expected="$1"; shift
    "$MODBOX" test "$@" >/dev/null 2>&1
    local rc=$?
    if [[ $rc -eq $expected ]]; then
        pass "test $* → exit $expected"
    else
        fail "test $* → expected exit $expected, got $rc"
    fi
}

echo "  ── unary file operators ──"
chk 0 -f "$WORK/file"
chk 1 -f "$WORK/dir"
chk 0 -d "$WORK/dir"
chk 0 -e "$WORK/file"
chk 1 -e "$WORK/missing"
chk 0 -L "$WORK/link"
chk 1 -L "$WORK/file"
chk 0 -s "$WORK/file"
chk 1 -d "$WORK/missing"

echo "  ── string operators ──"
chk 0 -n "hello"
chk 1 -n ""
chk 1 -z "hello"
chk 0 -z ""
chk 0 "a" = "a"
chk 1 "a" = "b"
chk 0 "a" != "b"
chk 1 "a" != "a"
chk 0 "nonempty"
chk 1 ""

echo "  ── integer operators ──"
chk 0 5 -eq 5
chk 1 5 -ne 5
chk 0 5 -gt 3
chk 1 5 -gt 5
chk 0 3 -lt 5
chk 0 4 -ge 4
chk 0 4 -le 4

echo "  ── file comparison operators ──"
chk 0 "$WORK/file" -ef "$WORK/file"
chk 1 "$WORK/file" -ef "$WORK/dir"

echo "  ── grouping, negation and logic ──"
chk 0 \( -f "$WORK/file" -a -d "$WORK/dir" \)
chk 1 \( -f "$WORK/file" -a -d "$WORK/file" \)
chk 0 ! -e "$WORK/missing"
chk 0 1 -eq 1 -o 2 -eq 3
chk 1 1 -eq 2 -a 2 -eq 3
chk 0 \( ! -e "$WORK/missing" \) -a -f "$WORK/file"

echo "  ── error / edge cases ──"
chk 1
chk 0 -f
chk 2 x y
chk 0 -f
chk 2 abc -eq 1

echo "  ── --help and --version ──"
assert_cmd_pat 'Usage:' test --help
assert_cmd_pat 'modbox' test --version
