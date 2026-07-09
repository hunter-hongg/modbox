SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo "── help ────────────────────────────────────"

assert_cmd_pat "Usage:" help
