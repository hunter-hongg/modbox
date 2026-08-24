SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── free ──────────────────────────────────────"

echo " ── basic output (non-empty, has Mem/Swap headers) ──"
assert_cmd_pat 'Mem:' free
assert_cmd_pat 'Swap:' free

echo " ── -h human-readable ──"
assert_cmd_pat '[0-9]+\.[0-9]+[KMG]' free -h

echo " ── --si SI units ──"
assert_cmd_pat '[0-9]+\.[0-9]+[KkMGT]B' free --si

echo " ── -t total row ──"
assert_cmd_pat 'total:' free -t

echo " ── -o old format (no available column) ──"
assert_cmd_not_pat 'available' free -o

echo " ── --json valid JSON ──"
result=$("$MODBOX" free --json 2>/dev/null)
if echo "$result" | python3 -m json.tool >/dev/null 2>&1; then
    pass "free --json (valid JSON)"
else
    fail "free --json — expected valid JSON"
fi

echo " ── --json structure ──"
assert_cmd_pat '"mem"' free --json
assert_cmd_pat '"swap"' free --json
assert_cmd_pat '"total"' free --json
assert_cmd_pat '"used"' free --json

echo " ── --help shows usage ──"
assert_cmd_pat 'Usage:' free --help

echo " ── --version shows version ──"
assert_cmd_pat 'free \(modbox\) 1\.0' free --version

echo " ── unknown option errors ──"
assert_cmd_pat_stderr 'invalid option' free --bogus

echo " ── positional arguments rejected ──"
assert_cmd_pat_stderr 'unexpected argument' free /tmp
