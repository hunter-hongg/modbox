SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── printf ─────────────────────────────────"

echo "  ── basic format ──"
assert_cmd "hello 42 Z" printf '%s %d %c' hello 42 Z

echo "  ── escapes ──"
assert_cmd "$(printf 'a\tb')" printf 'a\tb'

echo "  ── percent ──"
assert_cmd "100%" printf '%d%%' 100

echo "  ── --help ──"
assert_cmd_pat 'Usage:' printf --help
