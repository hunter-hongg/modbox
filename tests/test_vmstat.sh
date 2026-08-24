SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── vmstat ───────────────────────────────────────"

echo "  ── basic output (non-empty, has header) ──"
result=$("$MODBOX" vmstat 2>/dev/null)
if [[ -n "$result" ]]; then
    pass "vmstat (output non-empty)"
else
    fail "vmstat — expected non-empty output"
fi

echo "  ── output contains procs or memory headers ──"
assert_cmd_pat 'procs|memory|swap' vmstat

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' vmstat --help

echo "  ── --version shows version ──"
assert_cmd_pat 'vmstat \(modbox\) 1\.0' vmstat --version

echo "  ── delay/count produces multiple lines ──"
result=$("$MODBOX" vmstat 1 2 2>/dev/null)
line_count=$(echo "$result" | wc -l)
if [[ "$line_count" -ge 2 ]]; then
    pass "vmstat 1 2 (at least $line_count lines)"
else
    fail "vmstat 1 2 — expected ≥2 lines, got $line_count"
fi

echo "  ── -a flag shows active/inactive memory ──"
assert_cmd_pat 'active|inact' vmstat -a

echo "  ── invalid option errors ──"
assert_cmd_pat_stderr 'invalid option' vmstat --bogus
