SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── du ──────────────────────────────────────"

# Setup test directory
mkdir -p "$TMPDIR"/du_dir/sub
printf 'hello' > "$TMPDIR"/du_dir/a.txt
printf 'world' > "$TMPDIR"/du_dir/sub/b.txt

echo "  ── basic du (default 1K blocks) ──"
assert_cmd_pat 'du_dir$' du "$TMPDIR"/du_dir
assert_cmd_pat 'du_dir/sub$' du "$TMPDIR"/du_dir

echo "  ── -b: bytes ──"
assert_cmd_pat 'du_dir$' du -b "$TMPDIR"/du_dir

echo "  ── -h: human-readable ──"
assert_cmd_pat 'K' du -h "$TMPDIR"/du_dir

echo "  ── -s: summarize ──"
result=$("$MODBOX" du -s "$TMPDIR"/du_dir 2>/dev/null | wc -l)
if [[ "$result" == "1" ]]; then
    pass "du -s → exactly 1 line"
else
    fail "du -s — expected 1 line, got $result"
fi

echo "  ── -a: all files ──"
assert_cmd_pat 'a.txt' du -a "$TMPDIR"/du_dir 2>/dev/null
assert_cmd_pat 'b.txt' du -a "$TMPDIR"/du_dir 2>/dev/null

echo "  ── -c: total ──"
assert_cmd_pat 'total' du -c "$TMPDIR"/du_dir "$TMPDIR"/du_dir/sub 2>/dev/null

echo "  ── -d 0: max-depth 0 ──"
assert_cmd_not_pat 'du_dir/sub' du -d 0 "$TMPDIR"/du_dir 2>/dev/null

echo "  ── -k: 1K blocks ──"
assert_cmd_pat 'du_dir$' du -k "$TMPDIR"/du_dir

echo "  ── --si ──"
assert_cmd_pat 'du_dir$' du --si "$TMPDIR"/du_dir

echo "  ── --exclude ──"
assert_cmd_pat 'du_dir$' du --exclude='*.txt' "$TMPDIR"/du_dir

echo "  ── -t: threshold ──"
assert_cmd_pat 'du_dir$' du -t 1K "$TMPDIR"/du_dir

echo "  ── --time ──"
assert_cmd_pat 'du_dir$' du --time "$TMPDIR"/du_dir 2>/dev/null

echo "  ── help ──"
assert_cmd_pat 'Usage:' du --help
