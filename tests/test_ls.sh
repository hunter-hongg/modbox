SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── ls ──────────────────────────────────────"

mkdir -p "$TMPDIR"/ls_dir
touch    "$TMPDIR"/ls_dir/regular.txt
touch    "$TMPDIR"/ls_dir/.hidden
touch    "$TMPDIR"/ls_dir/backup~
touch    "$TMPDIR"/ls_dir/exec.sh
chmod +x "$TMPDIR"/ls_dir/exec.sh
mkdir    "$TMPDIR"/ls_dir/subdir
# Create file with non-graphic byte in its name (for -b escape test)
python3 -c "open('$TMPDIR/ls_dir/\x01file','w')" 2>/dev/null || touch "$TMPDIR"/ls_dir/ctrl

# ls tests need to run from the target directory (pre-existing path behavior
# — lstat() uses bare filenames, not full paths)
cd "$TMPDIR"/ls_dir

echo "  ── basic ls ──"
assert_cmd_pat 'regular\.txt' ls 2>/dev/null
assert_cmd_pat 'subdir'       ls 2>/dev/null

echo "  ── -a : show all ──"
assert_cmd_pat '\.hidden' ls -a 2>/dev/null
# ls output is space-separated, so match for ". " or ".  " (dot followed by spaces)
assert_cmd_pat '\.  '     ls -a 2>/dev/null
assert_cmd_pat '\.\. '    ls -a 2>/dev/null

echo "  ── -A : almost all — no . .. ──"
assert_cmd_pat '\.hidden' ls -A 2>/dev/null
# With -A, . and .. should NOT appear; check that no entry starts with just dot-space
assert_cmd_not_pat '(^| )\.\.?  *' ls -A 2>/dev/null

echo "  ── -l : long format ──"
assert_cmd_pat 'regular\.txt' ls -l 2>/dev/null
assert_cmd_pat '^d'           ls -l 2>/dev/null   # subdir starts with d
assert_cmd_pat '^-'           ls -l 2>/dev/null   # regular file starts with -
assert_cmd_pat 'exec\.sh'     ls -l 2>/dev/null   # executable

echo "  ── -la : long + all ──"
assert_cmd_pat '\.hidden' ls -la 2>/dev/null
assert_cmd_pat '^d'       ls -la 2>/dev/null

echo "  ── -B : ignore backups ──"
assert_cmd_not_pat 'backup~' ls -B 2>/dev/null

echo "  ── -l --author ──"
# --author adds extra user column (user user group)
assert_cmd_pat 'regular\.txt' ls -l --author 2>/dev/null

echo "  ── -b : escape non-graphic ──"
assert_cmd_pat '\\001' ls -b 2>/dev/null

echo "  ── --color=always ──"
assert_cmd_pat $'\x1b\[' ls --color=always 2>/dev/null

echo "  ── --colorful ──"
assert_cmd_pat $'\x1b\[' ls --colorful 2>/dev/null

echo "  ── --colorful -l : long format multi-color ──"
assert_cmd_pat $'\x1b\[' ls --colorful -l 2>/dev/null

echo "  ── --colorful -l has more ANSI than --color=always -l ──"
colorful_count=$("$MODBOX" ls --colorful -l 2>/dev/null | grep -o $'\x1b\[' | wc -l)
always_count=$("$MODBOX" ls --color=always -l 2>/dev/null | grep -o $'\x1b\[' | wc -l)
if [ "$colorful_count" -gt "$always_count" ]; then
    pass "ls --colorful -l → $colorful_count ANSI > --color=always -l → $always_count"
else
    fail "ls --colorful -l → $colorful_count ANSI, expected > $always_count"
fi

echo "  ── --block-size ──"
assert_cmd_pat 'reg' ls -l --block-size=K 2>/dev/null

echo "  ── multiple directories ──"
assert_cmd_pat 'regular' ls "$TMPDIR"/ls_dir /tmp 2>/dev/null

echo "  ── non-existent directory error ──"
assert_cmd_pat_stderr "No such file" ls "$TMPDIR"/nonexistent_dir

echo "  ── -d : list directory itself ──"
assert_cmd_pat '\.$'       ls -d . 2>/dev/null           # just "." (no contents)
assert_cmd_not_pat 'regular' ls -d . 2>/dev/null          # should NOT list contents

echo "  ── -d -l : long format of directory itself ──"
assert_cmd_pat '^d'        ls -dl . 2>/dev/null           # starts with 'd' for directory
assert_cmd_pat '\.$'       ls -dl . 2>/dev/null           # ends with "."

echo "  ── -d with regular file ──"
assert_cmd_pat 'regular\.txt' ls -d "$TMPDIR"/ls_dir/regular.txt 2>/dev/null
assert_cmd_not_pat 'No such file' ls -d "$TMPDIR"/ls_dir/regular.txt 2>/dev/null

echo "  ── -d with multiple directories ──"
assert_cmd_pat 'subdir' ls -d . subdir 2>/dev/null

echo "  ── -d -l with multiple directories ──"
assert_cmd_pat 'subdir$' ls -dl . subdir 2>/dev/null

echo "  ── -r : reverse sort ──"
assert_cmd_pat 'regular' ls -r 2>/dev/null

echo "  ── -U : unsorted (directory order) ──"
assert_cmd_pat 'regular' ls -U 2>/dev/null

echo "  ── -r reverses alphabetical order ──"
# Create dedicated dir with known sort order
mkdir -p "$TMPDIR"/ls_rev
touch "$TMPDIR"/ls_rev/a.txt "$TMPDIR"/ls_rev/b.txt "$TMPDIR"/ls_rev/c.txt
normal_first=$("$MODBOX" ls "$TMPDIR"/ls_rev 2>/dev/null | awk '{print $1}')
reversed_first=$("$MODBOX" ls -r "$TMPDIR"/ls_rev 2>/dev/null | awk '{print $1}')
if [ "$normal_first" = "a.txt" ]; then
    pass "ls normal order a.txt first"
else
    fail "ls normal order expected a.txt first got [$normal_first]"
fi
if [ "$reversed_first" = "c.txt" ]; then
    pass "ls -r reverse order c.txt first"
else
    fail "ls -r reverse order expected c.txt first got [$reversed_first]"
fi

echo "  ── -rU : unsorted takes precedence over reverse ──"
unsorted_output=$("$MODBOX" ls -U "$TMPDIR"/ls_rev 2>/dev/null)
rev_unsorted_output=$("$MODBOX" ls -rU "$TMPDIR"/ls_rev 2>/dev/null)
if [ "$unsorted_output" = "$rev_unsorted_output" ]; then
    pass "ls -rU = ls -U (unsorted takes precedence)"
else
    fail "ls -rU expected same as ls -U, got different output"
fi

echo "  ── -1 : one file per line ──"
# Output should have one file per line (count lines)
one_output=$("$MODBOX" ls -1 "$TMPDIR"/ls_dir 2>/dev/null)
one_lines=$(echo "$one_output" | wc -l)
# There are 6 entries in ls_dir (excluding . and ..)
if [ "$one_lines" -ge 4 ]; then
    pass "ls -1 → $one_lines lines (at least 4 entries)"
else
    fail "ls -1 → only $one_lines lines, expected >= 4"
fi

echo "  ── -1 with single file shows one line ──"
one_single=$("$MODBOX" ls -1 "$TMPDIR"/ls_dir/regular.txt 2>/dev/null)
one_single_lines=$(echo "$one_single" | wc -l)
if [ "$one_single_lines" -eq 1 ]; then
    pass "ls -1 single file → 1 line"
else
    fail "ls -1 single file → $one_single_lines lines, expected 1"
fi

# Create a symlink for -F testing
ln -sf "$TMPDIR"/ls_dir/subdir "$TMPDIR"/ls_dir/symlink_to_dir

echo "  ── -F : classify — dir gets / ──"
assert_cmd_pat 'subdir/' ls -F "$TMPDIR"/ls_dir 2>/dev/null

echo "  ── -F : classify — executable gets * ──"
assert_cmd_pat 'exec\.sh\*' ls -F "$TMPDIR"/ls_dir 2>/dev/null

echo "  ── -F : classify — symlink gets @ ──"
assert_cmd_pat 'symlink_to_dir@' ls -F "$TMPDIR"/ls_dir 2>/dev/null

echo "  ── -F : classify — regular file has no suffix ──"
assert_cmd_pat 'regular\.txt[^*/@]' ls -F "$TMPDIR"/ls_dir 2>/dev/null

echo "  ── -1F : one per line with classify ──"
one_f_output=$("$MODBOX" ls -1F "$TMPDIR"/ls_dir 2>/dev/null)
one_f_lines=$(echo "$one_f_output" | wc -l)
if echo "$one_f_output" | grep -qE 'subdir/'; then
    pass "ls -1F → one per line with classify indicator"
else
    fail "ls -1F — expected subdir/ in output"
fi

echo "  ── -F -l : long format with classify ──"
assert_cmd_pat 'subdir/' ls -Fl "$TMPDIR"/ls_dir 2>/dev/null

echo "  ── --tui falls back when not TTY ──"
tui_output=$("$MODBOX" ls --tui "$TMPDIR"/ls_dir 2>/dev/null)
if printf '%s' "$tui_output" | grep -qE 'regular\.txt'; then
    pass "ls --tui non-TTY → plain output with regular.txt"
else
    fail "ls --tui non-TTY → missing regular.txt in output"
fi

cd "$TMPDIR"
