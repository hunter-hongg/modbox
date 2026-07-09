SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── find ─────────────────────────────────────"

mkdir -p "$TMPDIR"/find_dir/sub
mkdir -p "$TMPDIR"/find_dir/empty_dir
printf 'hello world\n'        > "$TMPDIR"/find_dir/a.txt
printf '42\n'                 > "$TMPDIR"/find_dir/b.txt
printf 'test content\n'       > "$TMPDIR"/find_dir/sub/c.txt
printf ''                     > "$TMPDIR"/find_dir/empty.txt
ln -sf "a.txt"                "$TMPDIR"/find_dir/link.ln

echo "  ── basic: find with single path ──"
assert_cmd_pat 'find_dir/a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt'

echo "  ── -name glob pattern ──"
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'b*'

echo "  ── -maxdepth 0 (only starting dir itself) ──"
assert_cmd_pat 'find_dir$' find "$TMPDIR"/find_dir -maxdepth 0
assert_cmd_not_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 0

echo "  ── -maxdepth 1 (immediate children) ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt'
assert_cmd_not_pat 'sub/c\.txt' find "$TMPDIR"/find_dir -maxdepth 1

echo "  ── -maxdepth 2 (recurse into sub) ──"
assert_cmd_pat 'sub/c\.txt' find "$TMPDIR"/find_dir -maxdepth 2 -name 'c.txt'

echo "  ── -mindepth 1 (skip starting dir) ──"
assert_cmd_not_pat "^$TMPDIR/find_dir$" find "$TMPDIR"/find_dir -mindepth 1 -maxdepth 1

echo "  ── -type f (regular files) ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -type f
assert_cmd_not_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 1 -type f

echo "  ── -type d (directories) ──"
assert_cmd_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 1 -type d

echo "  ── -type l (symlinks) ──"
assert_cmd_pat 'link\.ln' find "$TMPDIR"/find_dir -maxdepth 1 -type l

echo "  ── -empty (empty file) ──"
assert_cmd_pat 'empty\.txt' find "$TMPDIR"/find_dir -maxdepth 3 -empty -type f

echo "  ── -empty (empty directory) ──"
assert_cmd_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 3 -empty -type d

echo "  ── -empty does not match non-empty ──"
assert_cmd_not_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -empty

echo "  ── combined: -type f -name ──"
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -type f -name 'b*'
assert_cmd_not_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 1 -type f -name 'b*'

echo "  ── multiple starting points ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir/a.txt "$TMPDIR"/find_dir/b.txt -maxdepth 0
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir/a.txt "$TMPDIR"/find_dir/b.txt -maxdepth 0

echo "  ── -print (explicit) ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt' -print

echo "  ── default action is -print ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt'
# Also check that a file that only matches via -print doesn't crash
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'b.txt' -print

echo "  ── error: nonexistent path on stderr ──"
assert_cmd_pat_stderr 'No such file' find /nonexistent_path_xyz

echo "  ── -exec echo {} ; (per-file) ──"
# exec directly: this should work if our fork/exec is correct
mkdir -p "$TMPDIR"/find_exec_test
printf 'hello exec\n' > "$TMPDIR"/find_exec_test/x.txt
printf 'hello exec\n' > "$TMPDIR"/find_exec_test/y.txt
assert_cmd_pat 'x\.txt' find "$TMPDIR"/find_exec_test -maxdepth 1 -type f -exec echo {} ";"

echo "  ── -exec echo {} + (batch) ──"
result=$(echo "$MODBOX" find "$TMPDIR"/find_exec_test -maxdepth 1 -type f -exec echo {} "+" 2>/dev/null)
assert_cmd_pat 'x\.txt' find "$TMPDIR"/find_exec_test -maxdepth 1 -type f -exec echo {} "+"

echo "  ── no starting point defaults to . ──"
cd "$TMPDIR"/find_dir
assert_cmd_pat 'a\.txt' find -maxdepth 1 -name 'a.txt'
cd /

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' find --help

echo "  ── -delete (empty file) ──"
cp "$TMPDIR"/find_dir/empty.txt "$TMPDIR"/find_dir/to_delete.txt
assert_cmd_pat 'to_delete' find "$TMPDIR"/find_dir -maxdepth 1 -name 'to_delete.txt' -print
if "$MODBOX" find "$TMPDIR"/find_dir -maxdepth 1 -name 'to_delete.txt' -delete; then
    pass "find -delete (file) → exit 0"
else
    fail "find -delete (file) → expected exit 0"
fi
if [ -f "$TMPDIR"/find_dir/to_delete.txt ]; then
    fail "find -delete (file) — file still exists after -delete"
else
    pass "find -delete (file) — file successfully removed"
fi
