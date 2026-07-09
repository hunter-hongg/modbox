echo ""
echo "── ln ──────────────────────────────────────"

echo "source content" > "$TMPDIR"/ln_src.txt
mkdir -p "$TMPDIR"/ln_dir

echo "  ── file → file ──"
"$MODBOX" ln "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_dst.txt
assert_cmd "source content" cat "$TMPDIR"/ln_dst.txt

echo "  ── file → existing directory ──"
"$MODBOX" ln "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_dir/
if [[ -f "$TMPDIR"/ln_dir/ln_src.txt ]]; then
    pass "ln file to directory — ln_src.txt found in target dir"
else
    fail "ln file to directory — ln_src.txt not found in target dir"
fi
assert_cmd "source content" cat "$TMPDIR"/ln_dir/ln_src.txt

echo "  ── -v : verbose ──"
out=$("$MODBOX" ln -v "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_v_dst.txt 2>/dev/null || true)
if echo "$out" | grep -qE "'.*ln_v_dst.*' ->"; then
    pass "ln -v → verbose output"
else
    fail "ln -v — expected verbose output, got [$out]"
fi

echo "  ── -f : force ──"
echo "dummy" > "$TMPDIR"/ln_existing.txt
"$MODBOX" ln -f "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_existing.txt
assert_cmd "source content" cat "$TMPDIR"/ln_existing.txt

echo "  ── error: non-existent source ──"
assert_cmd_pat_stderr "No such file" ln "$TMPDIR"/nonexistent "$TMPDIR"/ln_err

echo "  ── error: directory as source ──"
assert_cmd_pat_stderr "not a regular" ln "$TMPDIR"/ln_dir "$TMPDIR"/ln_dir_link

echo "  ── -s : symbolic link to regular file ──"
"$MODBOX" ln -s "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_sym.txt
if [[ -L "$TMPDIR"/ln_sym.txt ]]; then
    pass "ln -s file → symlink created"
else
    fail "ln -s file — not a symlink"
fi
assert_cmd "source content" cat "$TMPDIR"/ln_sym.txt

echo "  ── -s : symbolic link to directory ──"
"$MODBOX" ln -s "$TMPDIR"/ln_dir "$TMPDIR"/ln_sym_dir
if [[ -L "$TMPDIR"/ln_sym_dir ]]; then
    pass "ln -s directory → symlink created"
else
    fail "ln -s directory — not a symlink"
fi
if [[ -d "$TMPDIR"/ln_sym_dir ]]; then
    pass "ln -s directory → resolves to directory"
else
    fail "ln -s directory — does not resolve"
fi

echo "  ── -s : dangling symlink (source doesn't exist) ──"
"$MODBOX" ln -s "$TMPDIR"/nonexistent_target "$TMPDIR"/ln_dangling
if [[ -L "$TMPDIR"/ln_dangling ]]; then
    pass "ln -s dangling → symlink created"
else
    fail "ln -s dangling — not a symlink"
fi
# readlink should show the target path
target=$(readlink "$TMPDIR"/ln_dangling)
if [[ "$target" == */nonexistent_target ]]; then
    pass "ln -s dangling → readlink shows correct target"
else
    fail "ln -s dangling — readlink shows [$target]"
fi

echo "  ── -s -f : force overwrite existing file ──"
echo "i exist" > "$TMPDIR"/ln_sym_force_target
"$MODBOX" ln -sf "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_sym_force_target
if [[ -L "$TMPDIR"/ln_sym_force_target ]]; then
    pass "ln -sf → replaced regular file with symlink"
else
    fail "ln -sf — not a symlink after force"
fi
assert_cmd "source content" cat "$TMPDIR"/ln_sym_force_target

echo "  ── -s -v : verbose symbolic link ──"
out=$("$MODBOX" ln -s -v "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_sym_v.txt || true)
if echo "$out" | grep -qE "'.*ln_sym_v.*' ->"; then
    pass "ln -s -v → verbose output"
else
    fail "ln -s -v — expected verbose output, got [$out]"
fi

echo "  ── -s to existing directory destination ──"
echo "other source" > "$TMPDIR"/ln_src2.txt
"$MODBOX" ln -s "$TMPDIR"/ln_src2.txt "$TMPDIR"/ln_dir/
if [[ -L "$TMPDIR"/ln_dir/ln_src2.txt ]]; then
    pass "ln -s to dir → symlink created inside directory"
else
    fail "ln -s to dir — symlink not found inside directory"
fi
assert_cmd "other source" cat "$TMPDIR"/ln_dir/ln_src2.txt

echo "  ── -s to existing directory (different basename) ──"
"$MODBOX" ln -s "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_dir/sym_ref.txt
if [[ -L "$TMPDIR"/ln_dir/sym_ref.txt ]]; then
    pass "ln -s to dir (different name) → symlink created"
else
    fail "ln -s to dir (different name) — symlink not found"
fi
assert_cmd "source content" cat "$TMPDIR"/ln_dir/sym_ref.txt

echo "  ── -s -f : force overwrite existing symlink ──"
"$MODBOX" ln -s -f "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_sym.txt
if [[ -L "$TMPDIR"/ln_sym.txt ]]; then
    pass "ln -s -f overwrite symlink → still a symlink"
else
    fail "ln -s -f overwrite symlink — not a symlink"
fi
assert_cmd "source content" cat "$TMPDIR"/ln_sym.txt

echo "  ── -s -f : force overwrite dangling symlink ──"
"$MODBOX" ln -s -f "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_dangling
if [[ -L "$TMPDIR"/ln_dangling ]]; then
    pass "ln -s -f overwrite dangling → symlink replaced"
else
    fail "ln -s -f overwrite dangling — not a symlink"
fi
assert_cmd "source content" cat "$TMPDIR"/ln_dangling

echo "  ── -n : no-dereference (do not follow symlink to dir) ──"
mkdir -p "$TMPDIR"/ln_n_dir
ln -s "$TMPDIR"/ln_n_dir "$TMPDIR"/ln_n_link
"$MODBOX" ln -n -s -f "$TMPDIR/ln_src.txt" "$TMPDIR/ln_n_link" 2>/dev/null || true
if [[ -L "$TMPDIR"/ln_n_link && "$(readlink "$TMPDIR"/ln_n_link)" == "$TMPDIR/ln_src.txt" ]]; then
    pass "ln -n -s → created link at path, did not follow symlink to dir"
else
    fail "ln -n -s — expected link at path, got $(readlink "$TMPDIR"/ln_n_link 2>/dev/null)"
fi

echo "  ── -f : force overwrites existing file with link ──"
echo "replace_me" > "$TMPDIR"/ln_force_existing.txt
"$MODBOX" ln -f "$TMPDIR"/ln_src.txt "$TMPDIR"/ln_force_existing.txt 2>/dev/null || true
assert_cmd "source content" cat "$TMPDIR"/ln_force_existing.txt

echo "  ── -f : force on directory destination creates link inside ──"
dst_dir="$TMPDIR"/ln_force_dir
mkdir -p "$dst_dir"
"$MODBOX" ln -f "$TMPDIR"/ln_src.txt "$dst_dir/" 2>/dev/null || true
assert_cmd "source content" cat "$dst_dir/ln_src.txt"

echo "  ── -L : logical — dereference symlink source before hard link ──"
echo "symlink_target" > "$TMPDIR"/ln_L_target.txt
ln -sf "$TMPDIR"/ln_L_target.txt "$TMPDIR"/ln_L_sym.txt
"$MODBOX" ln -L "$TMPDIR"/ln_L_sym.txt "$TMPDIR"/ln_L_hard.txt 2>/dev/null || true
if [[ -f "$TMPDIR"/ln_L_hard.txt && ! -L "$TMPDIR"/ln_L_hard.txt ]]; then
    pass "ln -L → created regular file (not symlink)"
else
    fail "ln -L — expected regular file, got symlink or missing"
fi
assert_cmd "symlink_target" cat "$TMPDIR"/ln_L_hard.txt

echo "  ── -L with -s: no effect (symlink ignores -L) ──"
echo "L_with_s" > "$TMPDIR"/ln_Ls_target.txt
"$MODBOX" ln -L -s "$TMPDIR"/ln_Ls_target.txt "$TMPDIR"/ln_Ls_sym.txt 2>/dev/null || true
if [[ -L "$TMPDIR"/ln_Ls_sym.txt ]]; then
    pass "ln -L -s → symlink created (not affected by -L)"
else
    fail "ln -L -s — expected symlink"
fi
assert_cmd "L_with_s" cat "$TMPDIR"/ln_Ls_sym.txt

echo "  ── -L -v : verbose shows original source path ──"
out=$("$MODBOX" ln -L -v "$TMPDIR"/ln_L_sym.txt "$TMPDIR"/ln_Lv_hard.txt 2>/dev/null || true)
if echo "$out" | grep -qE "ln_Lv_hard.*->"; then
    pass "ln -L -v → verbose output"
else
    fail "ln -L -v — expected verbose output, got [$out]"
fi

echo "  ── -i : interactive prompt before overwrite (uses script to allocate pty) ──"
echo "orig" > "$TMPDIR"/ln_i_dst
if command -v script >/dev/null 2>&1; then
    # send 'n' to cancel
    printf "n\n" | script -q -c "$MODBOX ln -f -i $TMPDIR/ln_src.txt $TMPDIR/ln_i_dst" /dev/null 2>/dev/null || true
    assert_cmd "orig" cat "$TMPDIR"/ln_i_dst
    # send 'y' to confirm
    printf "y\n" | script -q -c "$MODBOX ln -f -i $TMPDIR/ln_src.txt $TMPDIR/ln_i_dst" /dev/null 2>/dev/null || true
    assert_cmd "source content" cat "$TMPDIR"/ln_i_dst
else
    pass "ln -i tests skipped (no script utility)"
fi
