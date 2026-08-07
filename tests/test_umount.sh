SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── umount ────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' umount --help

echo "  ── --version ──"
assert_cmd_pat 'umount \(modbox\) 1\.0' umount --version

echo "  ── --fake umount preview ──"
assert_cmd_pat 'umount /tmp' umount --fake /tmp

echo "  ── --fake with -l lazy ──"
assert_cmd_pat 'umount' umount --fake -l /tmp

echo "  ── --fake with -f force ──"
assert_cmd_pat 'umount' umount --fake -f /tmp

echo "  ── no args ──"
assert_cmd_pat 'Usage:' umount 2>/dev/null || assert_cmd_pat_stderr 'missing' umount

echo "  ── nonexistent mount target ──"
# This may fail differently depending on the system, so just check it errors
result=$("$MODBOX" umount "$TMPDIR"/nonexistent_umount_dir_xyz_12345 2>&1)
if [[ -n "$result" ]]; then
  pass "umount nonexistent target (outputs error)"
else
  fail "umount nonexistent target — expected error output"
fi

echo "  ── --fake with device path ──"
assert_cmd_pat 'umount /dev/sda1' umount --fake /dev/sda1
