SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── chattr ────────────────────────────────────────"

echo "  ── setup ──"
echo "normal file" > "$TMPDIR"/chattr_rw.txt
echo "readonly file" > "$TMPDIR"/chattr_ro.txt
chmod 444 "$TMPDIR"/chattr_ro.txt
echo "executable file" > "$TMPDIR"/chattr_exec.txt
chmod 755 "$TMPDIR"/chattr_exec.txt
mkdir -p "$TMPDIR"/chattr_dir/sub
echo "nested" > "$TMPDIR"/chattr_dir/sub/file.txt

echo "  ── --help ──"
assert_cmd_pat 'Usage:' chattr --help

echo "  ── missing operand ──"
assert_cmd_pat_stderr 'missing operand' chattr

echo "  ── mode specification required ──"
assert_cmd_pat_stderr 'missing mode specification' chattr "$TMPDIR"/chattr_rw.txt

echo "  ── nonexistent file ──"
assert_cmd_pat_stderr 'No such file' chattr +i /nonexistent/path

echo "  ── invalid attribute letter ──"
assert_cmd_pat_stderr 'unknown attribute' chattr +z "$TMPDIR"/chattr_rw.txt

echo "  ── set +i (immutable) ──"
chattr +i "$TMPDIR"/chattr_rw.txt 2>/dev/null || true

echo "  ── set +a (append-only) ──"
chattr +a "$TMPDIR"/chattr_exec.txt 2>/dev/null || true

echo "  ── multiple attributes +ia ──"
chattr +ia "$TMPDIR"/chattr_rw.txt 2>/dev/null || true

echo "  ── exact set (=i) ──"
chattr =i "$TMPDIR"/chattr_rw.txt 2>/dev/null || true

echo "  ── remove attribute (-i) ──"
chattr -i "$TMPDIR"/chattr_rw.txt 2>/dev/null || true

echo "  ── recursive operation (-R) ──"
chattr -R +V "$TMPDIR"/chattr_dir 2>/dev/null || true

echo "  ── suppress errors (-f) ──"
chattr -f +i "$TMPDIR"/nonexistentfile 2>&1 | grep -q "cannot access" && fail "suppress failed" || pass "suppress worked"

echo "  ── preserve root check ──"
OUTPUT=$($MODBOX chattr --preserve-root -R +i / 2>&1)
echo "$OUTPUT" | grep -q "dangerous to operate recursively on" || fail "preserve-root check failed"

echo "  ── all tests done ──"
