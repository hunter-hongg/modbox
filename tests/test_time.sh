SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── time ───────────────────────────────────────"

echo "  ── default format goes to stderr ──"
assert_cmd_pat_stderr 'elapsed' time true

echo "  ── portable format ──"
assert_cmd_pat_stderr '^real ' time -p true

echo "  ── portable includes user and sys ──"
result=$("$MODBOX" time -p true 2>&1 >/dev/null)
if echo "$result" | grep -q '^user ' && echo "$result" | grep -q '^sys '; then
    pass "time -p (user/sys lines)"
else
    fail "time -p — expected user/sys lines, got [$result]"
fi

echo "  ── custom format expands specifiers ──"
result=$("$MODBOX" time -f 'CMD=%C RC=%x' true 2>&1 >/dev/null)
if [[ "$result" == "CMD=true RC=0" ]]; then
    pass "time -f (%C/%x)"
else
    fail "time -f — expected [CMD=true RC=0] got [$result]"
fi

echo "  ── verbose report ──"
assert_cmd_pat_stderr 'Maximum resident set size' time -v true

echo "  ── exit status is passed through ──"
"$MODBOX" time -p false >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "time (exit passthrough)"
else
    fail "time — expected child exit 1 to pass through"
fi

echo "  ── nonexistent command exits 127 ──"
"$MODBOX" time nosuchcmd_modbox_xyz >/dev/null 2>&1
if [[ $? -eq 127 ]]; then
    pass "time nosuchcmd (exit 127)"
else
    fail "time nosuchcmd — expected exit 127"
fi

echo "  ── -o writes stats to a file ──"
outfile="$TMPDIR/time_out.txt"
"$MODBOX" time -o "$outfile" -p true >/dev/null 2>&1
if grep -q '^real ' "$outfile"; then
    pass "time -o (writes file)"
else
    fail "time -o — expected 'real' line in $outfile"
fi

echo "  ── -a appends to the output file ──"
"$MODBOX" time -a -o "$outfile" -p true >/dev/null 2>&1
count=$(grep -c '^real ' "$outfile")
if [[ "$count" -eq 2 ]]; then
    pass "time -a (appends)"
else
    fail "time -a — expected 2 'real' lines got [$count]"
fi

echo "  ── missing command errors ──"
assert_cmd_pat_stderr 'missing program to run' time

echo "  ── --help ──"
assert_cmd_pat 'Usage:' time --help

echo "  ── --version ──"
assert_cmd_pat 'time \(modbox\) 1.0' time --version
