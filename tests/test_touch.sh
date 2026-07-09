echo ""
echo "── touch ────────────────────────────────────"

echo "  ── create new file ──"
assert_cmd "" touch "$TMPDIR"/touch_new
test -f "$TMPDIR"/touch_new && pass "touch_new exists" || fail "touch_new not created"

echo "  ── update existing ──"
touch -t 202001010000 "$TMPDIR"/touch_new
assert_cmd "" touch "$TMPDIR"/touch_new
now=$(date +%Y)
mtime_year=$(stat -c '%y' "$TMPDIR"/touch_new | cut -d- -f1)
if [ "$mtime_year" = "$(date +%Y)" ]; then
    pass "touch updated mtime to current year"
else
    fail "touch — expected mtime year $(date +%Y) got [$mtime_year]"
fi

echo "  ── -c no-create (nonexistent) ──"
assert_cmd "" touch -c "$TMPDIR"/touch_noc
test -f "$TMPDIR"/touch_noc && fail "touch -c created file" || pass "touch -c did not create"

echo "  ── -r reference ──"
touch "$TMPDIR"/touch_ref
touch -t 202101010000 "$TMPDIR"/touch_ref
touch "$TMPDIR"/touch_target
assert_cmd "" touch -r "$TMPDIR"/touch_ref "$TMPDIR"/touch_target
ref_mtime=$(stat -c '%Y' "$TMPDIR"/touch_ref)
tgt_mtime=$(stat -c '%Y' "$TMPDIR"/touch_target)
if [ "$ref_mtime" = "$tgt_mtime" ]; then
    pass "touch -r copied timestamps"
else
    fail "touch -r — expected mtime [$ref_mtime] got [$tgt_mtime]"
fi

echo "  ── -a only access time ──"
touch -t 202201010000 "$TMPDIR"/touch_atime
sleep 1
assert_cmd "" touch -a "$TMPDIR"/touch_atime
atime=$(stat -c '%X' "$TMPDIR"/touch_atime)
mtime=$(stat -c '%Y' "$TMPDIR"/touch_atime)
if [ "$atime" != "$mtime" ]; then
    pass "touch -a changed atime only"
else
    fail "touch -a — atime and mtime are equal [$atime]"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' touch --help

echo "  ── multiple files ──"
assert_cmd "" touch "$TMPDIR"/touch_x "$TMPDIR"/touch_y
