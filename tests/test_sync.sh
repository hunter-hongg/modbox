SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sync ────────────────────────────────────"

echo "  ── help ──"
assert_cmd_pat 'Usage:' sync --help

echo "  ── version ──"
assert_cmd_pat 'sync \(modbox\)' sync --version

echo "  ── sync all filesystems (no args) ──"
assert_cmd "" sync

echo "  ── sync existing file ──"
echo "test content" > "$TMPDIR"/sync_test.txt
assert_cmd "" sync "$TMPDIR"/sync_test.txt

echo "  ── sync multiple files ──"
echo "test1" > "$TMPDIR"/sync_test1.txt
echo "test2" > "$TMPDIR"/sync_test2.txt
assert_cmd "" sync "$TMPDIR"/sync_test1.txt "$TMPDIR"/sync_test2.txt

echo "  ── sync non-existent file (error) ──"
assert_cmd_pat_stderr 'cannot open' sync "$TMPDIR"/nonexistent_file.txt
