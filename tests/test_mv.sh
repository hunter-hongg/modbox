echo ""
echo "── mv ──────────────────────────────────────"

echo "source content" > "$TMPDIR"/mv_src.txt
echo "another source" > "$TMPDIR"/mv_src2.txt
mkdir -p "$TMPDIR"/mv_dir/sub
echo "nested" > "$TMPDIR"/mv_dir/sub/file.txt

echo "  ── file → file (rename) ──"
"$MODBOX" mv "$TMPDIR"/mv_src.txt "$TMPDIR"/mv_dst.txt
# Source should be gone
if [[ ! -f "$TMPDIR"/mv_src.txt ]]; then
    pass "mv file → file — source removed"
else
    fail "mv file → file — source still exists"
fi
# Destination should have the content
assert_cmd "source content" cat "$TMPDIR"/mv_dst.txt

echo "  ── file → existing directory ──"
# Recreate source first
echo "file to dir" > "$TMPDIR"/mv_src.txt
mkdir -p "$TMPDIR"/mv_dir_target
"$MODBOX" mv "$TMPDIR"/mv_src.txt "$TMPDIR"/mv_dir_target/ 2>/dev/null || true
if [[ -f "$TMPDIR"/mv_dir_target/mv_src.txt ]]; then
    pass "mv file to directory — file found in target dir"
else
    fail "mv file to directory — file not found in target dir"
fi
if [[ ! -f "$TMPDIR"/mv_src.txt ]]; then
    pass "mv file to directory — source removed"
else
    fail "mv file to directory — source still exists"
fi
assert_cmd "file to dir" cat "$TMPDIR"/mv_dir_target/mv_src.txt

echo "  ── directory → directory (rename) ──"
"$MODBOX" mv "$TMPDIR"/mv_dir "$TMPDIR"/mv_dir_renamed 2>/dev/null || true
if [[ -d "$TMPDIR"/mv_dir_renamed ]]; then
    pass "mv directory → directory — renamed dir exists"
else
    fail "mv directory → directory — renamed dir not found"
fi
if [[ ! -d "$TMPDIR"/mv_dir ]]; then
    pass "mv directory → directory — source removed"
else
    fail "mv directory → directory — source still exists"
fi
assert_cmd "nested" cat "$TMPDIR"/mv_dir_renamed/sub/file.txt

echo "  ── directory → existing directory ──"
mkdir -p "$TMPDIR"/mv_dir2/sub
echo "nested2" > "$TMPDIR"/mv_dir2/sub/file2.txt
mkdir -p "$TMPDIR"/mv_parent_dir
"$MODBOX" mv "$TMPDIR"/mv_dir2 "$TMPDIR"/mv_parent_dir/ 2>/dev/null || true
if [[ -d "$TMPDIR"/mv_parent_dir/mv_dir2 ]]; then
    pass "mv directory to existing dir — dir found inside target"
else
    fail "mv directory to existing dir — dir not found inside target"
fi
assert_cmd "nested2" cat "$TMPDIR"/mv_parent_dir/mv_dir2/sub/file2.txt

echo "  ── multiple files → directory ──"
echo "multi1" > "$TMPDIR"/mv_multi1.txt
echo "multi2" > "$TMPDIR"/mv_multi2.txt
mkdir -p "$TMPDIR"/mv_multi_dst
"$MODBOX" mv "$TMPDIR"/mv_multi1.txt "$TMPDIR"/mv_multi2.txt "$TMPDIR"/mv_multi_dst/ 2>/dev/null || true
assert_cmd "multi1" cat "$TMPDIR"/mv_multi_dst/mv_multi1.txt
assert_cmd "multi2" cat "$TMPDIR"/mv_multi_dst/mv_multi2.txt
if [[ ! -f "$TMPDIR"/mv_multi1.txt && ! -f "$TMPDIR"/mv_multi2.txt ]]; then
    pass "mv multiple files — sources removed"
else
    fail "mv multiple files — some sources still exist"
fi

echo "  ── overwrite existing destination ──"
echo "original content" > "$TMPDIR"/mv_overwrite_dst.txt
echo "new content" > "$TMPDIR"/mv_overwrite_src.txt
"$MODBOX" mv "$TMPDIR"/mv_overwrite_src.txt "$TMPDIR"/mv_overwrite_dst.txt
assert_cmd "new content" cat "$TMPDIR"/mv_overwrite_dst.txt
if [[ ! -f "$TMPDIR"/mv_overwrite_src.txt ]]; then
    pass "mv overwrite — source removed"
else
    fail "mv overwrite — source still exists"
fi

echo "  ── -n: no-clobber does not overwrite ──"
echo "keep me" > "$TMPDIR"/mv_n_src.txt
echo "original" > "$TMPDIR"/mv_n_dst.txt
"$MODBOX" mv -n "$TMPDIR"/mv_n_src.txt "$TMPDIR"/mv_n_dst.txt
assert_cmd "original" cat "$TMPDIR"/mv_n_dst.txt
if [[ -f "$TMPDIR"/mv_n_src.txt ]]; then
    pass "mv -n — src preserved (not overwritten)"
else
    fail "mv -n — src removed despite no-clobber"
fi

echo "  ── -n: no-clobber overrides -i ──"
echo "keep too" > "$TMPDIR"/mv_ni_src.txt
echo "orig too" > "$TMPDIR"/mv_ni_dst.txt
echo "n" | "$MODBOX" mv -ni "$TMPDIR"/mv_ni_src.txt "$TMPDIR"/mv_ni_dst.txt
assert_cmd "orig too" cat "$TMPDIR"/mv_ni_dst.txt

echo "  ── -i: interactive prompts on overwrite ──"
echo "interactive content" > "$TMPDIR"/mv_i_src.txt
echo "original content" > "$TMPDIR"/mv_i_dst.txt
echo "n" | "$MODBOX" mv -i "$TMPDIR"/mv_i_src.txt "$TMPDIR"/mv_i_dst.txt
assert_cmd "original content" cat "$TMPDIR"/mv_i_dst.txt
if [[ -f "$TMPDIR"/mv_i_src.txt ]]; then
    pass "mv -i — src preserved when answering n"
else
    fail "mv -i — src removed despite answering n"
fi

echo "  ── -i: interactive allows overwrite on y ──"
echo "will be moved" > "$TMPDIR"/mv_iy_src.txt
echo "will be replaced" > "$TMPDIR"/mv_iy_dst.txt
echo "y" | "$MODBOX" mv -i "$TMPDIR"/mv_iy_src.txt "$TMPDIR"/mv_iy_dst.txt
assert_cmd "will be moved" cat "$TMPDIR"/mv_iy_dst.txt
if [[ ! -f "$TMPDIR"/mv_iy_src.txt ]]; then
    pass "mv -i — src removed when answering y"
else
    fail "mv -i — src still present after answering y"
fi

echo "  ── error: non-existent source ──"
assert_cmd_pat_stderr "No such file" mv "$TMPDIR"/mv_nonexistent "$TMPDIR"/mv_err

echo "  ── error: multiple sources with non-directory dest ──"
echo "a" > "$TMPDIR"/mv_multi_err_a.txt
echo "b" > "$TMPDIR"/mv_multi_err_b.txt
echo "not_a_dir" > "$TMPDIR"/mv_not_a_dir.txt
assert_cmd_pat_stderr "not a directory" mv "$TMPDIR"/mv_multi_err_a.txt "$TMPDIR"/mv_multi_err_b.txt "$TMPDIR"/mv_not_a_dir.txt

echo "  ── error: move directory into itself ──"
mkdir -p "$TMPDIR"/mv_self/subdir
assert_cmd_pat_stderr "Invalid argument" mv "$TMPDIR"/mv_self "$TMPDIR"/mv_self/subdir

# Verify source still exists (destination directory is unharmed)
if [[ -d "$TMPDIR"/mv_self ]]; then
    pass "mv directory into itself — source preserved"
else
    fail "mv directory into itself — source removed"
fi

echo "  ── -f: force overwrite existing destination ──"
echo "forced content" > "$TMPDIR"/mv_f_src.txt
echo "original" > "$TMPDIR"/mv_f_dst.txt
"$MODBOX" mv -f "$TMPDIR"/mv_f_src.txt "$TMPDIR"/mv_f_dst.txt
assert_cmd "forced content" cat "$TMPDIR"/mv_f_dst.txt

echo "  ── -v: verbose output ──"
echo "verbose src" > "$TMPDIR"/mv_v_src.txt
out=$("$MODBOX" mv -v "$TMPDIR"/mv_v_src.txt "$TMPDIR"/mv_v_dst.txt 2>/dev/null || true)
if echo "$out" | grep -qE "' -> '"; then
    pass "mv -v → verbose output shows '->'"
else
    fail "mv -v — expected '->' in output, got [$out]"
fi
assert_cmd "verbose src" cat "$TMPDIR"/mv_v_dst.txt

echo "  ── -u: update when source is newer ──"
echo "newer content" > "$TMPDIR"/mv_u_newer_src.txt
echo "older content" > "$TMPDIR"/mv_u_newer_dst.txt
touch -t 202001010000 "$TMPDIR"/mv_u_newer_dst.txt
sleep 1
"$MODBOX" mv -u "$TMPDIR"/mv_u_newer_src.txt "$TMPDIR"/mv_u_newer_dst.txt
assert_cmd "newer content" cat "$TMPDIR"/mv_u_newer_dst.txt
if [[ ! -f "$TMPDIR"/mv_u_newer_src.txt ]]; then
    pass "mv -u (source newer) — src removed"
else
    fail "mv -u (source newer) — src still exists"
fi

echo "  ── -u: skip when source is older ──"
echo "skip src" > "$TMPDIR"/mv_u_skip_src.txt
echo "keep dst" > "$TMPDIR"/mv_u_skip_dst.txt
touch -t 202001010000 "$TMPDIR"/mv_u_skip_src.txt
"$MODBOX" mv -u "$TMPDIR"/mv_u_skip_src.txt "$TMPDIR"/mv_u_skip_dst.txt
assert_cmd "keep dst" cat "$TMPDIR"/mv_u_skip_dst.txt
if [[ -f "$TMPDIR"/mv_u_skip_src.txt ]]; then
    pass "mv -u (source older) — src preserved"
else
    fail "mv -u (source older) — src removed despite -u"
fi

echo "  ── -b: backup existing destination ──"
echo "new data" > "$TMPDIR"/mv_b_src.txt
echo "backup data" > "$TMPDIR"/mv_b_dst.txt
"$MODBOX" mv -b "$TMPDIR"/mv_b_src.txt "$TMPDIR"/mv_b_dst.txt
assert_cmd "new data" cat "$TMPDIR"/mv_b_dst.txt
assert_cmd "backup data" cat "$TMPDIR"/mv_b_dst.txt~

echo "  ── -t: target-directory ──"
echo "t1" > "$TMPDIR"/mv_t_a.txt
echo "t2" > "$TMPDIR"/mv_t_b.txt
mkdir -p "$TMPDIR"/mv_t_dir
"$MODBOX" mv -t "$TMPDIR"/mv_t_dir "$TMPDIR"/mv_t_a.txt "$TMPDIR"/mv_t_b.txt
assert_cmd "t1" cat "$TMPDIR"/mv_t_dir/mv_t_a.txt
assert_cmd "t2" cat "$TMPDIR"/mv_t_dir/mv_t_b.txt
if [[ ! -f "$TMPDIR"/mv_t_a.txt && ! -f "$TMPDIR"/mv_t_b.txt ]]; then
    pass "mv -t — sources removed"
else
    fail "mv -t — sources still exist"
fi

echo "  ── -t: error when target does not exist ──"
echo "x" > "$TMPDIR"/mv_t_err.txt
assert_cmd_pat_stderr "No such file" mv -t "$TMPDIR"/mv_t_nonexistent "$TMPDIR"/mv_t_err.txt

echo "  ── -t: error when target is not a directory ──"
echo "not_a_dir" > "$TMPDIR"/mv_t_notdir
assert_cmd_pat_stderr "not a directory" mv -t "$TMPDIR"/mv_t_notdir "$TMPDIR"/mv_t_err.txt

echo "  ── -f overrides -i (no prompt) ──"
echo "fi content" > "$TMPDIR"/mv_fi_src.txt
echo "fi original" > "$TMPDIR"/mv_fi_dst.txt
echo "n" | "$MODBOX" mv -fi "$TMPDIR"/mv_fi_src.txt "$TMPDIR"/mv_fi_dst.txt
assert_cmd "fi content" cat "$TMPDIR"/mv_fi_dst.txt

echo "  ── -n overrides -f (no-clobber wins over force) ──"
echo "nf content" > "$TMPDIR"/mv_nf_src.txt
echo "nf original" > "$TMPDIR"/mv_nf_dst.txt
"$MODBOX" mv -nf "$TMPDIR"/mv_nf_src.txt "$TMPDIR"/mv_nf_dst.txt
assert_cmd "nf original" cat "$TMPDIR"/mv_nf_dst.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' mv --help
