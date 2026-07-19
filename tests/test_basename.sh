SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── basename ──────────────────────────────────"

echo "  ── strip directory ──"
assert_cmd 'bin' basename /usr/bin

echo "  ── strip directory and suffix ──"
assert_cmd 'bin' basename /usr/bin .bin

echo "  ── no directory ──"
assert_cmd 'file.txt' basename file.txt

echo "  ── strip suffix ──"
assert_cmd 'file' basename file.txt .txt

echo "  ── trailing slash ──"
assert_cmd 'dir' basename /path/to/dir/

echo "  ── root path ──"
assert_cmd '' basename /

echo "  ── single component ──"
assert_cmd 'foo' basename foo

echo "  ── -a multiple args ──"
assert_cmd_pat 'usr' basename -a /usr /bin

echo "  ── --help ──"
assert_cmd_pat 'Usage:' basename --help

echo "  ── --version ──"
assert_cmd_pat 'basename \(modbox\) 1\.0' basename --version
