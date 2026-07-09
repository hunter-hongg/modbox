SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── uname ────────────────────────────────────"

echo "  ── default (kernel name) ──"
assert_cmd "$(uname -s)" uname

echo "  ── -s : kernel name ──"
assert_cmd "$(uname -s)" uname -s

echo "  ── -n : nodename ──"
assert_cmd "$(uname -n)" uname -n

echo "  ── -r : kernel release ──"
assert_cmd "$(uname -r)" uname -r

echo "  ── -v : kernel version ──"
assert_cmd "$(uname -v)" uname -v

echo "  ── -m : machine ──"
assert_cmd "$(uname -m)" uname -m

echo "  ── -o : operating system ──"
assert_cmd "$(uname -o)" uname -o

echo "  ── -a : all (contains kernel name) ──"
assert_cmd_pat "$(uname -s)" uname -a
assert_cmd_pat "$(uname -n)" uname -a
assert_cmd_pat "$(uname -r)" uname -a
assert_cmd_pat "$(uname -m)" uname -a

echo "  ── multiple flags ──"
assert_cmd "$(printf '%s %s\n' "$(uname -s)" "$(uname -r)")" uname -s -r

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' uname --help
