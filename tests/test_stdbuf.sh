SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── stdbuf ──────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' stdbuf --help

echo "  ── --version shows version ──"
assert_cmd_pat 'stdbuf \(modbox\)' stdbuf --version

echo "  ── -i L line buffered ──"
assert_cmd_pat '^hello$' stdbuf -i L echo hello

echo "  ── -o 0 unbuffered output ──"
assert_cmd_pat '^hello$' stdbuf -o 0 echo hello

echo "  ── -e L line buffered error ──"
assert_cmd_pat '^hello$' stdbuf -e L echo hello

echo "  ── --input=L line buffered ──"
assert_cmd_pat '^hello$' stdbuf --input=L echo hello

echo "  ── --output=0 unbuffered ──"
assert_cmd_pat '^hello$' stdbuf --output=0 echo hello

echo "  ── missing command errors ──"
assert_cmd_pat_stderr 'missing command' stdbuf -i L

echo "  ── invalid mode errors ──"
assert_cmd_pat_stderr 'invalid input mode' stdbuf -i X echo hello
