echo ""
echo "── chgrp ──────────────────────────────────────────"

echo "  ── setup ──"
echo "testchgrp" > "$TMPDIR"/chgrp_a.txt
touch "$TMPDIR"/chgrp_b.txt
mkdir -p "$TMPDIR"/chgrp_dir/sub
echo "nested" > "$TMPDIR"/chgrp_dir/sub/file.txt

echo "  ── --help ──"
assert_cmd_pat 'Usage:' chgrp --help

echo "  ── missing operand ──"
assert_cmd_pat_stderr 'missing operand' chgrp

echo "  ── invalid group ──"
assert_cmd_pat_stderr 'invalid group' chgrp nosuchgroup99xx "$TMPDIR"/chgrp_a.txt

echo "  ── change group by name ──"
"$MODBOX" chgrp "$(id -gn)" "$TMPDIR"/chgrp_a.txt 2>/dev/null || true
group=$(stat -c '%g' "$TMPDIR"/chgrp_a.txt)
if [[ "$group" == "$MY_GID" ]]; then
    pass "chgrp <group> → group is $group"
else
    fail "chgrp <group> — expected gid $MY_GID, got $group"
fi

echo "  ── change group by numeric gid ──"
"$MODBOX" chgrp "$MY_GID" "$TMPDIR"/chgrp_b.txt 2>/dev/null || true
group=$(stat -c '%g' "$TMPDIR"/chgrp_b.txt)
if [[ "$group" == "$MY_GID" ]]; then
    pass "chgrp <gid> → group is $group"
else
    fail "chgrp <gid> — expected gid $MY_GID, got $group"
fi

echo "  ── -v: verbose output ──"
echo "grpverbose" > "$TMPDIR"/chgrp_verbose.txt
assert_cmd_pat "changed group" chgrp -v "$MY_GID" "$TMPDIR"/chgrp_verbose.txt

echo "  ── -c: no output when unchanged ──"
echo "grpcc" > "$TMPDIR"/chgrp_cc.txt
out=$("$MODBOX" chgrp -c "$MY_GID" "$TMPDIR"/chgrp_cc.txt 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "chgrp -c — no output when unchanged"
else
    fail "chgrp -c — unexpected output: [$out]"
fi


echo "  ── -f: silent (suppress error) ──"
out=$("$MODBOX" chgrp -f "$MY_GID" "$TMPDIR"/chgrp_nonexistent 2>&1 || true)
if [[ -z "$out" ]]; then
    pass "chgrp -f → no error output"
else
    fail "chgrp -f — unexpected output: [$out]"
fi

echo "  ── --reference: copy group from ref file ──"
echo "refsrc" > "$TMPDIR"/chgrp_refsrc.txt
echo "reftgt" > "$TMPDIR"/chgrp_reftgt.txt
chgrp "$MY_GID" "$TMPDIR"/chgrp_refsrc.txt 2>/dev/null || true
"$MODBOX" chgrp --reference="$TMPDIR"/chgrp_refsrc.txt "$TMPDIR"/chgrp_reftgt.txt 2>/dev/null || true
group=$(stat -c '%g' "$TMPDIR"/chgrp_reftgt.txt)
if [[ "$group" == "$MY_GID" ]]; then
    pass "chgrp --reference → group is $group"
else
    fail "chgrp --reference — expected gid $MY_GID, got $group"
fi

echo "  ── -R: recursive ──"
"$MODBOX" chgrp -R "$MY_GID" "$TMPDIR"/chgrp_dir 2>/dev/null || true
dir_group=$(stat -c '%g' "$TMPDIR"/chgrp_dir)
file_group=$(stat -c '%g' "$TMPDIR"/chgrp_dir/sub/file.txt)
if [[ "$dir_group" == "$MY_GID" && "$file_group" == "$MY_GID" ]]; then
    pass "chgrp -R → dir=$dir_group file=$file_group"
else
    fail "chgrp -R — expected both $MY_GID, got dir=$dir_group file=$file_group"
fi

echo "  ── -Rv: recursive verbose ──"
mkdir -p "$TMPDIR"/chgrp_rvdir/sub
touch "$TMPDIR"/chgrp_rvdir/sub/f.txt
assert_cmd_pat "changed group" chgrp -Rv "$MY_GID" "$TMPDIR"/chgrp_rvdir
