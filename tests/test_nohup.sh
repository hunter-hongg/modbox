SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── nohup ─────────────────────────────────────"

echo "  ── runs command, passing through output (non-tty) ──"
# stdout is a pipe here (not a tty), so no nohup.out is created.
assert_cmd 'hello' nohup echo hello

echo "  ── forwards command exit status is not required, but command runs ──"
result=$("$MODBOX" nohup printf 'a\nb\n' 2>/dev/null || true)
if [[ "$result" == "$(printf 'a\nb')" ]]; then
    pass "nohup printf a b"
else
    fail "nohup printf a b — got [$result]"
fi

echo "  ── missing operand ──"
assert_cmd_pat_stderr 'missing operand' nohup

echo "  ── command not found → exit 127 ──"
"$MODBOX" nohup this_command_does_not_exist_xyz >/dev/null 2>&1
if [[ $? -eq 127 ]]; then
    pass "nohup nonexistent → exit 127"
else
    fail "nohup nonexistent — expected exit 127 got $?"
fi

echo "  ── unrecognized option ──"
assert_cmd_pat_stderr "unrecognized option" nohup --bogus

echo "  ── -- ends options, runs command ──"
assert_cmd 'world' nohup -- echo world

echo "  ── --help ──"
assert_cmd_pat 'Usage:' nohup --help

echo "  ── --version ──"
assert_cmd_pat 'nohup \(modbox\) 1.0' nohup --version
