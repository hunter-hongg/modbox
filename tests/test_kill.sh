SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── kill ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' kill --help

echo "  ── --version ──"
assert_cmd_pat 'modbox' kill --version

echo "  ── -l lists all signals ──"
assert_cmd_pat 'SIGHUP' kill -l

echo "  ── -l TERM prints signal name ──"
assert_cmd 'TERM' kill -l TERM

echo "  ── -l 15 prints signal name ──"
assert_cmd 'TERM' kill -l 15

echo "  ── -l KILL prints signal name ──"
assert_cmd 'KILL' kill -l KILL

echo "  ── -l HUP prints signal name ──"
assert_cmd 'HUP' kill -l HUP

echo "  ── -l SIGKILL strips SIG prefix ──"
assert_cmd 'KILL' kill -l SIGKILL

echo "  ── -0 self exits 0 (process exists) ──"
if "$MODBOX" kill -0 $$ </dev/null 2>/dev/null; then
    pass "-0 $$ → exit 0"
else
    fail "-0 $$ → expected exit 0"
fi

echo "  ── -0 nonexistent pid exits non-zero ──"
if "$MODBOX" kill -0 99999999 </dev/null 2>/dev/null; then
    fail "-0 99999999 → expected non-zero"
else
    pass "-0 99999999 → exit non-zero"
fi

echo "  ── no operand errors ──"
assert_cmd_pat_stderr 'missing operand' kill

echo "  ── invalid pid errors ──"
assert_cmd_pat_stderr 'invalid pid' kill -s TERM notapid
