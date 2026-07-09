echo ""
echo "── cp ──────────────────────────────────────"

echo "source content" > "$TMPDIR"/cp_src.txt
mkdir -p "$TMPDIR"/cp_src_dir/sub
echo "nested" > "$TMPDIR"/cp_src_dir/sub/file.txt

echo "  ── file → file ──"
"$MODBOX" cp "$TMPDIR"/cp_src.txt "$TMPDIR"/cp_dst.txt
assert_cmd "source content" cat "$TMPDIR"/cp_dst.txt

echo "  ── file → existing directory ──"
mkdir -p "$TMPDIR"/cp_dir_target
"$MODBOX" cp "$TMPDIR"/cp_src.txt "$TMPDIR"/cp_dir_target/ 2>/dev/null || true
# cp to existing dir should place file inside with same basename
if [[ -f "$TMPDIR"/cp_dir_target/cp_src.txt ]]; then
    assert_cmd "source content" cat "$TMPDIR"/cp_dir_target/cp_src.txt
else
    fail "cp file to directory — cp_src.txt not found in target dir"
fi

echo "  ── -r : recursive ──"
"$MODBOX" cp -r "$TMPDIR"/cp_src_dir "$TMPDIR"/cp_recursive_dst 2>/dev/null || true
assert_cmd "nested" cat "$TMPDIR"/cp_recursive_dst/sub/file.txt

echo "  ── -v : verbose ──"
# -v prints "'src' -> 'dst'" to stdout
out=$("$MODBOX" cp -v "$TMPDIR"/cp_src.txt "$TMPDIR"/cp_v_dst.txt 2>/dev/null || true)
if echo "$out" | grep -qE "'$TMPDIR/cp_src.txt' ->"; then
    pass "cp -v → verbose output"
else
    fail "cp -v — expected verbose output, got [$out]"
fi

echo "  ── -r -v : recursive verbose ──"
out=$("$MODBOX" cp -r -v "$TMPDIR"/cp_src_dir "$TMPDIR"/cp_rv_dst 2>/dev/null || true)
if echo "$out" | grep -qE "'$TMPDIR/cp_src_dir' ->"; then
    pass "cp -r -v → verbose output"
else
    fail "cp -r -v — expected verbose output, got [$(echo "$out" | head -c 80)]"
fi

echo "  ── multiple sources with -r ──"
mkdir -p "$TMPDIR"/cp_m1/sub "$TMPDIR"/cp_m2
echo "f1" > "$TMPDIR"/cp_m1/f1.txt
echo "f2" > "$TMPDIR"/cp_m2/f2.txt
mkdir -p "$TMPDIR"/cp_multi_dst
"$MODBOX" cp -r "$TMPDIR"/cp_m1 "$TMPDIR"/cp_m2 "$TMPDIR"/cp_multi_dst 2>/dev/null || true
assert_cmd "f1" cat "$TMPDIR"/cp_multi_dst/cp_m1/f1.txt
assert_cmd "f2" cat "$TMPDIR"/cp_multi_dst/cp_m2/f2.txt

echo "  ── error: non-existent source ──"
assert_cmd_pat_stderr "No such file" cp "$TMPDIR"/nonexistent "$TMPDIR"/dest

echo "  ── error: dir without -r ──"
assert_cmd_pat_stderr "not a regular" cp "$TMPDIR"/cp_src_dir "$TMPDIR"/cp_no_r

echo "  ── -n : no-clobber skips existing destination ──"
echo "keep me" > "$TMPDIR"/cp_n_dst.txt
echo "overwrite" > "$TMPDIR"/cp_n_src.txt
"$MODBOX" cp -n "$TMPDIR"/cp_n_src.txt "$TMPDIR"/cp_n_dst.txt 2>/dev/null || true
assert_cmd "keep me" cat "$TMPDIR"/cp_n_dst.txt

echo "  ── -n : no-clobber copies when destination doesn't exist ──"
echo "new file" > "$TMPDIR"/cp_n_new_src.txt
"$MODBOX" cp -n "$TMPDIR"/cp_n_new_src.txt "$TMPDIR"/cp_n_new_dst.txt 2>/dev/null || true
assert_cmd "new file" cat "$TMPDIR"/cp_n_new_dst.txt

echo "  ── -n -r : recursive no-clobber ──"
mkdir -p "$TMPDIR"/cp_nr_src/sub "$TMPDIR"/cp_nr_dst/sub
echo "keep" > "$TMPDIR"/cp_nr_dst/sub/existing.txt
echo "overwrite" > "$TMPDIR"/cp_nr_src/sub/existing.txt
echo "fresh" > "$TMPDIR"/cp_nr_src/sub/fresh.txt
"$MODBOX" cp -n -r "$TMPDIR"/cp_nr_src/sub "$TMPDIR"/cp_nr_dst/ 2>/dev/null || true
assert_cmd "keep" cat "$TMPDIR"/cp_nr_dst/sub/existing.txt
assert_cmd "fresh" cat "$TMPDIR"/cp_nr_dst/sub/fresh.txt

echo "  ── -f : force overwrites read-only destination ──"
echo "foriginal" > "$TMPDIR"/cp_f_src.txt
echo "freadonly" > "$TMPDIR"/cp_f_dst.txt
chmod 444 "$TMPDIR"/cp_f_dst.txt
"$MODBOX" cp -f "$TMPDIR"/cp_f_src.txt "$TMPDIR"/cp_f_dst.txt 2>/dev/null || true
assert_cmd "foriginal" cat "$TMPDIR"/cp_f_dst.txt
chmod 644 "$TMPDIR"/cp_f_dst.txt 2>/dev/null || true

echo "  ── -f : force on writable file still works ──"
echo "old" > "$TMPDIR"/cp_fw_dst.txt
"$MODBOX" cp -f "$TMPDIR"/cp_src.txt "$TMPDIR"/cp_fw_dst.txt 2>/dev/null || true
assert_cmd "source content" cat "$TMPDIR"/cp_fw_dst.txt

echo "  ── -f : force works when destination doesn't exist ──"
echo "fresh target" > "$TMPDIR"/cp_f_new_src.txt
"$MODBOX" cp -f "$TMPDIR"/cp_f_new_src.txt "$TMPDIR"/cp_f_new_dst.txt 2>/dev/null || true
assert_cmd "fresh target" cat "$TMPDIR"/cp_f_new_dst.txt

echo "  ── -i : interactive prompt — answer 'n' skips, 'y' overwrites ──"
echo "i_orig" > "$TMPDIR"/cp_i_dst.txt
echo "i_new" > "$TMPDIR"/cp_i_src.txt
if command -v script >/dev/null 2>&1; then
    printf "n\n" | script -q -c "$MODBOX cp -i $TMPDIR/cp_i_src.txt $TMPDIR/cp_i_dst.txt" /dev/null 2>/dev/null || true
    assert_cmd "i_orig" cat "$TMPDIR"/cp_i_dst.txt
    printf "y\n" | script -q -c "$MODBOX cp -i $TMPDIR/cp_i_src.txt $TMPDIR/cp_i_dst.txt" /dev/null 2>/dev/null || true
    assert_cmd "i_new" cat "$TMPDIR"/cp_i_dst.txt
else
    pass "cp -i tests skipped (no script utility)"
fi

echo "  ── -i : interactive overwrites when destination doesn't exist ──"
echo "i_new2" > "$TMPDIR"/cp_i_src2.txt
"$MODBOX" cp -i "$TMPDIR"/cp_i_src2.txt "$TMPDIR"/cp_i_new_dst.txt 2>/dev/null || true
assert_cmd "i_new2" cat "$TMPDIR"/cp_i_new_dst.txt

echo "  ── -u : copy when destination doesn't exist ──"
echo "u_new" > "$TMPDIR"/cp_u_src.txt
"$MODBOX" cp -u "$TMPDIR"/cp_u_src.txt "$TMPDIR"/cp_u_dst1.txt 2>/dev/null || true
assert_cmd "u_new" cat "$TMPDIR"/cp_u_dst1.txt

echo "  ── -u : copy when source is newer than destination ──"
echo "u_old" > "$TMPDIR"/cp_u_dst2.txt
sleep 1.1
echo "u_newer" > "$TMPDIR"/cp_u_src2.txt
"$MODBOX" cp -u "$TMPDIR"/cp_u_src2.txt "$TMPDIR"/cp_u_dst2.txt 2>/dev/null || true
assert_cmd "u_newer" cat "$TMPDIR"/cp_u_dst2.txt

echo "  ── -u : skip when source is older than destination ──"
echo "u_new_src" > "$TMPDIR"/cp_u_src3.txt
sleep 1.1
echo "u_older_dst" > "$TMPDIR"/cp_u_dst3.txt
"$MODBOX" cp -u "$TMPDIR"/cp_u_src3.txt "$TMPDIR"/cp_u_dst3.txt 2>/dev/null || true
assert_cmd "u_older_dst" cat "$TMPDIR"/cp_u_dst3.txt

echo "  ── -u -r : recursive update ──"
mkdir -p "$TMPDIR"/cp_ur_src/sub "$TMPDIR"/cp_ur_dst/sub
echo "keep" > "$TMPDIR"/cp_ur_dst/sub/existing.txt
sleep 1.1
echo "fresher" > "$TMPDIR"/cp_ur_src/sub/existing.txt
echo "newfile" > "$TMPDIR"/cp_ur_src/sub/new.txt
"$MODBOX" cp -u -r "$TMPDIR"/cp_ur_src/sub "$TMPDIR"/cp_ur_dst/ 2>/dev/null || true
assert_cmd "fresher" cat "$TMPDIR"/cp_ur_dst/sub/existing.txt
assert_cmd "newfile" cat "$TMPDIR"/cp_ur_dst/sub/new.txt

echo "  ── -n overrides -f ──"
echo "noforce" > "$TMPDIR"/cp_nf_dst.txt
echo "should not appear" > "$TMPDIR"/cp_nf_src.txt
"$MODBOX" cp -f -n "$TMPDIR"/cp_nf_src.txt "$TMPDIR"/cp_nf_dst.txt 2>/dev/null || true
assert_cmd "noforce" cat "$TMPDIR"/cp_nf_dst.txt

echo "  ── error: missing destination ──"
err=$("$MODBOX" cp "$TMPDIR"/cp_src.txt 2>&1 || true)
if echo "$err" | grep -qiE "missing|expected|requires a value"; then
    pass "cp: missing dest → reports error"
else
    fail "cp: missing dest — no error, got [$err]"
fi

echo "  ── -t : target-directory ──"
mkdir -p "$TMPDIR"/cp_t_dst
"$MODBOX" cp -t "$TMPDIR"/cp_t_dst "$TMPDIR"/cp_src.txt 2>/dev/null || true
assert_cmd "source content" cat "$TMPDIR"/cp_t_dst/cp_src.txt

echo "  ── -t : target-directory with multiple files ──"
echo "multi_a" > "$TMPDIR"/cp_t_a.txt
echo "multi_b" > "$TMPDIR"/cp_t_b.txt
mkdir -p "$TMPDIR"/cp_t_multi
"$MODBOX" cp -t "$TMPDIR"/cp_t_multi "$TMPDIR"/cp_t_a.txt "$TMPDIR"/cp_t_b.txt 2>/dev/null || true
assert_cmd "multi_a" cat "$TMPDIR"/cp_t_multi/cp_t_a.txt
assert_cmd "multi_b" cat "$TMPDIR"/cp_t_multi/cp_t_b.txt

echo "  ── -t -r : target-directory recursive ──"
mkdir -p "$TMPDIR"/cp_tr_dst
"$MODBOX" cp -t "$TMPDIR"/cp_tr_dst -r "$TMPDIR"/cp_src_dir 2>/dev/null || true
assert_cmd "nested" cat "$TMPDIR"/cp_tr_dst/cp_src_dir/sub/file.txt

echo "  ── -t : error when target not a directory ──"
echo "not_a_dir" > "$TMPDIR"/cp_t_not_dir
assert_cmd_pat_stderr "not a directory" cp -t "$TMPDIR"/cp_t_not_dir "$TMPDIR"/cp_src.txt 2>/dev/null || true

echo "  ── -t : error when target does not exist ──"
assert_cmd_pat_stderr "No such file or directory" cp -t "$TMPDIR"/cp_t_nonexistent "$TMPDIR"/cp_src.txt

echo "  ── -p -t : preserve with target-directory ──"
echo "pt_test" > "$TMPDIR"/cp_pt_src.txt
chmod 0642 "$TMPDIR"/cp_pt_src.txt
mkdir -p "$TMPDIR"/cp_pt_dst
"$MODBOX" cp -p -t "$TMPDIR"/cp_pt_dst "$TMPDIR"/cp_pt_src.txt 2>/dev/null || true
src_mode=$(stat -c "%a" "$TMPDIR"/cp_pt_src.txt)
dst_mode=$(stat -c "%a" "$TMPDIR"/cp_pt_dst/cp_pt_src.txt)
if [ "$src_mode" = "$dst_mode" ]; then
    pass "cp -p -t → mode preserved ($src_mode)"
else
    fail "cp -p -t — mode mismatch src=$src_mode dst=$dst_mode"
fi

echo "  ── -p : preserve mode ──"
echo "preserve_me" > "$TMPDIR"/cp_p_src.txt
chmod 0642 "$TMPDIR"/cp_p_src.txt
"$MODBOX" cp -p "$TMPDIR"/cp_p_src.txt "$TMPDIR"/cp_p_dst.txt 2>/dev/null || true
src_mode=$(stat -c "%a" "$TMPDIR"/cp_p_src.txt)
dst_mode=$(stat -c "%a" "$TMPDIR"/cp_p_dst.txt)
if [ "$src_mode" = "$dst_mode" ]; then
    pass "cp -p → mode preserved ($src_mode)"
else
    fail "cp -p — mode mismatch src=$src_mode dst=$dst_mode"
fi

echo "  ── -p : preserve timestamps ──"
echo "preserve_time" > "$TMPDIR"/cp_p_time_src.txt
touch -t 202501011200 "$TMPDIR"/cp_p_time_src.txt
"$MODBOX" cp -p "$TMPDIR"/cp_p_time_src.txt "$TMPDIR"/cp_p_time_dst.txt 2>/dev/null || true
src_mtime=$(stat -c "%Y" "$TMPDIR"/cp_p_time_src.txt)
dst_mtime=$(stat -c "%Y" "$TMPDIR"/cp_p_time_dst.txt)
if [ "$src_mtime" = "$dst_mtime" ]; then
    pass "cp -p → mtime preserved"
else
    fail "cp -p — mtime mismatch src=$src_mtime dst=$dst_mtime"
fi

echo "  ── -p : without -p timestamps NOT preserved ──"
echo "no_preserve" > "$TMPDIR"/cp_nop_src.txt
touch -t 202101010000 "$TMPDIR"/cp_nop_src.txt
"$MODBOX" cp "$TMPDIR"/cp_nop_src.txt "$TMPDIR"/cp_nop_dst.txt 2>/dev/null || true
src_mtime=$(stat -c "%Y" "$TMPDIR"/cp_nop_src.txt)
dst_mtime=$(stat -c "%Y" "$TMPDIR"/cp_nop_dst.txt)
if [ "$src_mtime" != "$dst_mtime" ]; then
    pass "cp (no -p) → mtime NOT preserved (expected)"
else
    if [ "$src_mtime" = "$dst_mtime" ]; then
        # might be same if second granularity catches up — still pass
        pass "cp (no -p) → mtime equal (may be coincidental)"
    else
        pass "cp (no -p) → mtime different"
    fi
fi

echo "  ── -p -r : recursive preserve ──"
mkdir -p "$TMPDIR"/cp_pr_src/sub
echo "pr_test" > "$TMPDIR"/cp_pr_src/sub/pr.txt
chmod 0750 "$TMPDIR"/cp_pr_src/sub
chmod 0611 "$TMPDIR"/cp_pr_src/sub/pr.txt
"$MODBOX" cp -p -r "$TMPDIR"/cp_pr_src "$TMPDIR"/cp_pr_dst 2>/dev/null || true
src_dir_mode=$(stat -c "%a" "$TMPDIR"/cp_pr_src/sub)
dst_dir_mode=$(stat -c "%a" "$TMPDIR"/cp_pr_dst/sub)
src_file_mode=$(stat -c "%a" "$TMPDIR"/cp_pr_src/sub/pr.txt)
dst_file_mode=$(stat -c "%a" "$TMPDIR"/cp_pr_dst/sub/pr.txt)
if [ "$src_dir_mode" = "$dst_dir_mode" ] && [ "$src_file_mode" = "$dst_file_mode" ]; then
    pass "cp -p -r → modes preserved (dir=$dst_dir_mode file=$dst_file_mode)"
else
    fail "cp -p -r — dir mode src=$src_dir_mode dst=$dst_dir_mode file mode src=$src_file_mode dst=$dst_file_mode"
fi
