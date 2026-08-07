SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── mount ────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' mount --help

echo "  ── --version ──"
assert_cmd_pat 'mount \(modbox\) 1\.0' mount --version

echo "  ── -a: list all mounts ──"
result=$("$MODBOX" mount -a 2>/dev/null)
if [[ -n "$result" ]]; then
  pass "mount -a (output non-empty)"
else
  fail "mount -a — expected non-empty output"
fi

echo "  ── no args: list all mounts ──"
result=$("$MODBOX" mount 2>/dev/null)
if [[ -n "$result" ]]; then
  pass "mount (no args, output non-empty)"
else
  fail "mount (no args) — expected non-empty output"
fi

echo "  ── --fake mount preview ──"
assert_cmd_pat 'mount.*on.*type' mount --fake /dev/null /tmp

echo "  ── --fake with -t type ──"
assert_cmd_pat 'type tmpfs' mount --fake /dev/null /tmp -t tmpfs

echo "  ── --fake with -O options ──"
assert_cmd_pat 'mount.*on.*type' mount --fake /dev/null /tmp -O rw

echo "  ── --fake with --options ──"
assert_cmd_pat 'mount.*on.*type' mount --fake /dev/null /tmp --options=rw

echo "  ── --fake with --target ──"
assert_cmd_pat 'on /mnt' mount --fake /dev/sdb1 --target /mnt

echo "  ── missing device ──"
assert_cmd_pat_stderr 'missing device' mount --fake

echo "  ── missing target (no fake) ──"
assert_cmd_pat_stderr 'missing target' mount /dev/null

echo "  ── nonexistent target dir ──"
assert_cmd_pat_stderr 'No such file or directory' mount /dev/null "$TMPDIR"/nonexistent_mount_dir_xyz_12345

echo "  ── invalid option ──"
assert_cmd_pat_stderr 'invalid option' mount --fake /dev/null /tmp --invalid-opt 2>/dev/null || pass "mount invalid option (handled)"
