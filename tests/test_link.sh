#!/usr/bin/env bash
#
# test_link.sh — Test the link command
#

# Source the test framework
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── link ──────────────────────────────────────"

echo "source content" > "$TMPDIR"/link_src.txt
mkdir -p "$TMPDIR"/link_dir

echo "  ── file → file ──"
"$MODBOX" link "$TMPDIR"/link_src.txt "$TMPDIR"/link_dst.txt
assert_cmd "source content" cat "$TMPDIR"/link_dst.txt

echo "  ── file → subdirectory ──"
"$MODBOX" link "$TMPDIR"/link_src.txt "$TMPDIR"/link_dir/link_test.txt
assert_cmd "source content" cat "$TMPDIR"/link_dir/link_test.txt

echo "  ── -v : verbose ──"
out=$("$MODBOX" link -v "$TMPDIR"/link_src.txt "$TMPDIR"/link_v_dst.txt 2>/dev/null || true)
if echo "$out" | grep -qE "'.*link_v_dst.*' linked to"; then
    pass "link -v → verbose output"
else
    fail "link -v — expected verbose output, got [$out]"
fi

echo "  ── error: non-existent source ──"
assert_cmd_pat_stderr "failed to access" link "$TMPDIR"/nonexistent "$TMPDIR"/link_err

echo "  ── error: non-existent destination directory ──"
assert_cmd_pat_stderr "failed to access directory" link "$TMPDIR"/link_src.txt "$TMPDIR"/nonexistentdir/link.txt

echo "  ── error: destination already exists ──"
echo "existing" > "$TMPDIR"/link_existing.txt
assert_cmd_pat_stderr "File exists" link "$TMPDIR"/link_src.txt "$TMPDIR"/link_existing.txt

echo "  ── hard link has same inode ──"
src_inode=$(stat -c "%i" "$TMPDIR"/link_src.txt)
dst_inode=$(stat -c "%i" "$TMPDIR"/link_dst.txt)
if [[ "$src_inode" == "$dst_inode" ]]; then
    pass "link → same inode (hard link)"
else
    fail "link — different inodes: src=$src_inode dst=$dst_inode"
fi

echo "  ── hard link to current directory ──"
"$MODBOX" link "$TMPDIR"/link_src.txt "$TMPDIR"/link_cwd.txt
assert_cmd "source content" cat "$TMPDIR"/link_cwd.txt
