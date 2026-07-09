SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── ps ────────────────────────────────────────"

echo "  ── default: shows PID,TTY,TIME,CMD headers ──"
assert_cmd_pat 'PID.*TTY.*TIME.*CMD' ps

echo "  ── -A : all processes ──"
assert_cmd_pat 'PID.*TTY.*TIME.*CMD' ps -A
assert_cmd_pat '^\s*1\s' ps -A

echo "  ── -e : all processes (same as -A) ──"
assert_cmd_pat 'PID.*TTY.*TIME.*CMD' ps -e
assert_cmd_pat '^\s*1\s' ps -e

echo "  ── -f : full format listing ──"
assert_cmd_pat 'UID.*PID.*PPID.*C.*STIME.*TTY.*TIME.*CMD' ps -f

echo "  ── -u : user-oriented format (BSD) ──"
assert_cmd_pat 'USER.*PID.*%CPU.*%MEM.*VSZ.*RSS.*TTY.*STAT.*START.*TIME.*COMMAND' ps -u

echo "  ── -a : processes except session leaders ──"
assert_cmd_pat 'PID.*TTY.*TIME.*CMD' ps -a

echo "  ── -x : lift tty restriction ──"
assert_cmd_pat 'PID.*TTY.*TIME.*CMD' ps -x

echo "  ── aux : BSD-style (all processes) ──"
assert_cmd_pat 'USER.*PID.*%CPU.*%MEM.*VSZ.*RSS.*TTY.*STAT.*START.*TIME.*COMMAND' ps aux
assert_cmd_pat '^\s*root\s' ps aux

echo "  ── -A -f : all processes with full format ──"
assert_cmd_pat 'UID.*PID.*PPID.*C.*STIME.*TTY.*TIME.*CMD' ps -A -f
assert_cmd_pat '^\s*root\s' ps -A -f

echo "  ── --help ──"
assert_cmd_pat 'Usage:' ps --help

echo "  ── error: invalid option ──"
assert_cmd_pat_stderr 'invalid option' ps --nonexistent

echo "  ── empty stdin (same as default) ──"
assert_cmd_pat 'PID.*TTY.*TIME.*CMD' ps < /dev/null
