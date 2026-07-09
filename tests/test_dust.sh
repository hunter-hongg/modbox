SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── dust ─────────────────────────────────────"

mkdir -p "$TMPDIR"/dust_dir/sub
printf 'hello' > "$TMPDIR"/dust_dir/a.txt
printf 'world' > "$TMPDIR"/dust_dir/sub/b.txt

echo "  ── basic dust ──"
assert_cmd_pat '█' dust "$TMPDIR"/dust_dir

echo "  ── -c: no color ──"
assert_cmd_pat '%' dust -c "$TMPDIR"/dust_dir

echo "  ── -d 0: depth 0 ──"
assert_cmd_not_pat 'sub' dust -c -d 0 "$TMPDIR"/dust_dir

echo "  ── -n: max lines ──"
result=$("$MODBOX" dust -c -n 1 "$TMPDIR"/dust_dir 2>/dev/null | wc -l)
if [[ "$result" == "1" ]]; then
    pass "dust -n 1 → exactly 1 line"
else
    fail "dust -n 1 — expected 1 line, got $result"
fi

echo "  ── -a: all files ──"
assert_cmd_pat 'a.txt' dust -c -a "$TMPDIR"/dust_dir

echo "  ── -b: bytes ──"
assert_cmd_pat '█' dust -c -b "$TMPDIR"/dust_dir

echo "  ── -H: si ──"
assert_cmd_pat 'kB' dust -c -H "$TMPDIR"/dust_dir

echo "  ── -X: exclude ──"
assert_cmd_pat '0%' dust -c -X '*.txt' "$TMPDIR"/dust_dir

echo "  ── help ──"
assert_cmd_pat 'Usage:' dust --help

echo "  ── ascending order (smallest first) ──"
# First line should be subdir (smallest), last line the root (largest)
lines=$("$MODBOX" dust -c "$TMPDIR"/dust_dir 2>/dev/null)
first=$(echo "$lines" | head -1)
last=$(echo "$lines" | tail -1)
if echo "$first" | grep -q 'dust_dir/sub$' && echo "$last" | grep -q 'dust_dir$'; then
    pass "dust → subdir first (smallest), root last (largest)"
else
    fail "dust — expected subdir first, root last"
fi
