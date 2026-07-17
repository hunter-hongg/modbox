SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── nice ───────────────────────────────────────"

echo "  ── no args prints current niceness ──"
assert_cmd_pat '^-?[0-9]+$' nice

echo "  ── default adjustment is 10 ──"
result=$("$MODBOX" nice "$MODBOX" nice 2>/dev/null || true)
if [[ "$result" == "10" ]]; then
    pass "nice (default +10)"
else
    fail "nice (default +10) — expected [10] got [$result]"
fi

echo "  ── -n 5 adjusts by 5 ──"
result=$("$MODBOX" nice -n 5 "$MODBOX" nice 2>/dev/null || true)
if [[ "$result" == "5" ]]; then
    pass "nice -n 5"
else
    fail "nice -n 5 — expected [5] got [$result]"
fi

echo "  ── -nN attached form ──"
result=$("$MODBOX" nice -n3 "$MODBOX" nice 2>/dev/null || true)
if [[ "$result" == "3" ]]; then
    pass "nice -n3"
else
    fail "nice -n3 — expected [3] got [$result]"
fi

echo "  ── obsolete -N form ──"
result=$("$MODBOX" nice -7 "$MODBOX" nice 2>/dev/null || true)
if [[ "$result" == "7" ]]; then
    pass "nice -7"
else
    fail "nice -7 — expected [7] got [$result]"
fi

echo "  ── --adjustment=N form ──"
result=$("$MODBOX" nice --adjustment=4 "$MODBOX" nice 2>/dev/null || true)
if [[ "$result" == "4" ]]; then
    pass "nice --adjustment=4"
else
    fail "nice --adjustment=4 — expected [4] got [$result]"
fi

echo "  ── accumulated adjustments ──"
result=$("$MODBOX" nice -n 3 -n 4 "$MODBOX" nice 2>/dev/null || true)
if [[ "$result" == "7" ]]; then
    pass "nice -n 3 -n 4 (accumulates)"
else
    fail "nice -n 3 -n 4 — expected [7] got [$result]"
fi

echo "  ── adjustment without command errors ──"
assert_cmd_pat_stderr 'a command must be given with an adjustment' nice -n 5

echo "  ── invalid adjustment errors ──"
assert_cmd_pat_stderr "invalid adjustment 'abc'" nice -n abc

echo "  ── nonexistent command exits 127 ──"
"$MODBOX" nice nosuchcmd_modbox_xyz >/dev/null 2>&1
if [[ $? -eq 127 ]]; then
    pass "nice nosuchcmd (exit 127)"
else
    fail "nice nosuchcmd — expected exit 127"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' nice --help

echo "  ── --version ──"
assert_cmd_pat 'nice \(modbox\) 1.0' nice --version