echo ""
echo "── dir ──────────────────────────────────────"

mkdir -p "$TMPDIR"/dir_dir
touch    "$TMPDIR"/dir_dir/regular.txt
touch    "$TMPDIR"/dir_dir/.hidden
touch    "$TMPDIR"/dir_dir/exec.sh
chmod +x "$TMPDIR"/dir_dir/exec.sh
mkdir    "$TMPDIR"/dir_dir/subdir

echo "  ── basic dir ──"
assert_cmd_pat 'regular\.txt' dir "$TMPDIR"/dir_dir
assert_cmd_pat 'subdir'       dir "$TMPDIR"/dir_dir
assert_cmd_pat 'exec\.sh'     dir "$TMPDIR"/dir_dir

echo "  ── dir -a : show all ──"
assert_cmd_pat '\.hidden' dir -a "$TMPDIR"/dir_dir

echo "  ── dir --help ──"
assert_cmd_pat 'Usage:' dir --help

echo "  ── dir nonexistent dir ──"
assert_cmd_pat_stderr "No such file" dir "$TMPDIR"/nope

echo ""
echo "── vdir ──────────────────────────────────────"

echo "  ── basic vdir (long format) ──"
assert_cmd_pat 'regular\.txt' vdir "$TMPDIR"/dir_dir
assert_cmd_pat '^d'           vdir "$TMPDIR"/dir_dir    # subdir
assert_cmd_pat '^-'           vdir "$TMPDIR"/dir_dir    # regular file

echo "  ── vdir --help ──"
assert_cmd_pat 'Usage:' vdir --help

echo "  ── vdir nonexistent dir ──"
assert_cmd_pat_stderr "No such file" vdir "$TMPDIR"/nope
