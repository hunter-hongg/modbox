echo ""
echo "── rm ───────────────────────────────────────"

echo "  ── single file ──"
touch "$TMPDIR"/rm_file
assert_cmd "" rm "$TMPDIR"/rm_file
if [ ! -f "$TMPDIR"/rm_file ]; then
    pass "rm removed file"
else
    fail "rm did not remove file"
fi

echo "  ── -f nonexistent (no error) ──"
assert_cmd "" rm -f "$TMPDIR"/rm_nonexistent

echo "  ── -r recursive ──"
mkdir -p "$TMPDIR"/rm_dir/sub
touch "$TMPDIR"/rm_dir/sub/file
assert_cmd "" rm -r "$TMPDIR"/rm_dir
if [ ! -d "$TMPDIR"/rm_dir ]; then
    pass "rm -r removed directory tree"
else
    fail "rm -r did not remove directory tree"
fi

echo "  ── -v verbose ──"
touch "$TMPDIR"/rm_verb
assert_cmd_pat "removed" rm -v "$TMPDIR"/rm_verb

echo "  ── -d empty dir ──"
mkdir "$TMPDIR"/rm_empty
assert_cmd "" rm -d "$TMPDIR"/rm_empty

echo "  ── multiple files ──"
touch "$TMPDIR"/rm_a "$TMPDIR"/rm_b
assert_cmd "" rm "$TMPDIR"/rm_a "$TMPDIR"/rm_b

echo "  ── help ──"
assert_cmd_pat 'Usage:' rm --help

echo "  ── dir without -r (error) ──"
mkdir "$TMPDIR"/rm_nr_dir
assert_cmd_pat_stderr "Is a directory" rm "$TMPDIR"/rm_nr_dir

echo "  ── --trash file ──"
touch "$TMPDIR"/rm_trash_file
assert_cmd "" rm --trash "$TMPDIR"/rm_trash_file
if [ ! -f "$TMPDIR"/rm_trash_file ] && [ -f "$HOME/.trash/rm_trash_file" ]; then
    pass "rm --trash moved file to ~/.trash"
    rm -f "$HOME/.trash/rm_trash_file"
else
    fail "rm --trash — file still at original or not in ~/.trash"
fi

echo "  ── --trash -v verbose ──"
touch "$TMPDIR"/rm_trash_verb
assert_cmd_pat "trashed" rm --trash -v "$TMPDIR"/rm_trash_verb
rm -f "$HOME/.trash/rm_trash_verb"

echo "  ── --trash dir with -r ──"
mkdir -p "$TMPDIR"/rm_trash_dir/sub
touch "$TMPDIR"/rm_trash_dir/sub/file
assert_cmd "" rm --trash -r "$TMPDIR"/rm_trash_dir
if [ ! -d "$TMPDIR"/rm_trash_dir ] && [ -d "$HOME/.trash/rm_trash_dir" ]; then
    pass "rm --trash -r moved directory to ~/.trash"
    rm -rf "$HOME/.trash/rm_trash_dir"
else
    fail "rm --trash -r — dir still at original or not in ~/.trash"
fi

echo "  ── --trash dir without -r (error) ──"
mkdir -p "$TMPDIR"/rm_trash_nr_dir
assert_cmd_pat_stderr "Is a directory" rm --trash "$TMPDIR"/rm_trash_nr_dir
rm -rf "$TMPDIR"/rm_trash_nr_dir

echo "  ── --trash collision handling ──"
touch "$TMPDIR"/rm_collision
echo "first" > "$HOME/.trash/rm_collision"
assert_cmd "" rm --trash "$TMPDIR"/rm_collision
if [ -f "$HOME/.trash/rm_collision" ] && [ -f "$HOME/.trash/rm_collision.1" ]; then
    pass "rm --trash created numbered variant on collision"
    rm -f "$HOME/.trash/rm_collision" "$HOME/.trash/rm_collision.1"
else
    fail "rm --trash — collision not handled correctly"
fi

echo "  ── --trash -f nonexistent (no error) ──"
assert_cmd "" rm --trash -f "$TMPDIR"/rm_trash_nonexistent

echo "  ── --trash -f force (no prompt) ──"
touch "$TMPDIR"/rm_trash_force
assert_cmd "" rm --trash -f "$TMPDIR"/rm_trash_force
if [ -f "$HOME/.trash/rm_trash_force" ]; then
    pass "rm --trash -f moved file silently"
    rm -f "$HOME/.trash/rm_trash_force"
else
    fail "rm --trash -f — file not in ~/.trash"
fi

echo "  ── --trash in help ──"
assert_cmd_pat "trash" rm --help

# Clean up trash directory after rm tests
rm -rf "$HOME/.trash"
