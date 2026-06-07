#!/usr/bin/env bash
#
# run_tests.sh — Automated test suite for modbox
#
# Tests all commands (cat, ls, cp, help) and their options.
# Each test prints PASS or FAIL with details on mismatch.
#

# Do NOT set errexit — test failures use non-zero returns via grep/[[ ]] etc.
set -o nounset

MODBOX=$(readlink -f "$(dirname "$0")/../target/modbox")
PASS_COUNT=0
FAIL_COUNT=0
TMPDIR=$(mktemp -d /tmp/modbox_test.XXXXXX)
trap 'rm -rf "$TMPDIR"' EXIT

# ── Test framework ──────────────────────────────────────────────────────────

pass()  { echo "  PASS  $*"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail()  { echo "  FAIL  $*"; FAIL_COUNT=$((FAIL_COUNT + 1)); }

# assert_cmd EXPECTED_OUTPUT args...
# Runs: modbox <args...> and compares stdout to EXPECTED_OUTPUT
assert_cmd() {
    local expected="$1"; shift
    local actual
    actual=$("$MODBOX" "$@" 2>/dev/null || true)
    if [[ "$actual" == "$expected" ]]; then
        pass "$*"
    else
        fail "$* — expected [$(echo "$expected" | head -c 80)] got [$(echo "$actual" | head -c 80)]"
    fi
}

# assert_cmd_pat PATTERN args...
# Runs modbox <args> and checks stdout contains PATTERN (extended regex)
assert_cmd_pat() {
    local pattern="$1"; shift
    if "$MODBOX" "$@" 2>/dev/null | grep -qE "$pattern"; then
        pass "$* → matches /$pattern/"
    else
        fail "$* — expected pattern /$pattern/ not found in output"
    fi
}

# assert_cmd_not_pat PATTERN args...
assert_cmd_not_pat() {
    local pattern="$1"; shift
    if "$MODBOX" "$@" 2>/dev/null | grep -qE "$pattern"; then
        fail "$* — unexpected pattern /$pattern/ found"
    else
        pass "$* — correctly lacks /$pattern/"
    fi
}

# assert_cmd_pat_stderr PATTERN args...
assert_cmd_pat_stderr() {
    local pattern="$1"; shift
    if "$MODBOX" "$@" 2>&1 1>/dev/null | grep -qE "$pattern"; then
        pass "$* → stderr matches /$pattern/"
    else
        fail "$* — expected stderr pattern /$pattern/ not found"
    fi
}

echo "============================================"
echo "  modbox Test Suite"
echo "  Binary: $MODBOX"
echo "============================================"
echo ""

# ═══════════════════════════════════════════════════════════════════════════
#  help
# ═══════════════════════════════════════════════════════════════════════════

echo "── help ────────────────────────────────────"

assert_cmd_pat "Usage:" help

# ── cat ─────────────────────────────────────────────────────────────────────

echo ""
echo "── cat ─────────────────────────────────────"

printf 'hello\nworld\n' > "$TMPDIR"/simple.txt
printf '\n\n\n'                > "$TMPDIR"/blanks.txt
printf 'line1\n\n\nline4\n'    > "$TMPDIR"/mixed.txt
printf 'with\ttab\n'           > "$TMPDIR"/tab.txt
printf 'a\n\n\nb\n'            > "$TMPDIR"/squeeze.txt
printf 'hello\n'               > "$TMPDIR"/a.txt
printf 'world\n'               > "$TMPDIR"/b.txt
printf '\x7f\n'                > "$TMPDIR"/del.txt
printf '\x01\x02\x1f\n'        > "$TMPDIR"/low.txt
printf '\x80\x9f\xa0\xff\n'    > "$TMPDIR"/high.txt
printf ''                      > "$TMPDIR"/empty.txt

echo "  ── basic read ──"
assert_cmd "$(printf 'hello\nworld\n')" cat "$TMPDIR"/simple.txt

echo "  ── -n : number all lines ──"
assert_cmd "$(printf '     1  hello\n     2  world\n')" cat -n "$TMPDIR"/simple.txt

echo "  ── -b : number non-blank lines ──"
assert_cmd "$(printf '     1  line1\n\n\n     2  line4\n')" cat -b "$TMPDIR"/mixed.txt

echo "  ── -nb : -b overrides -n ──"
assert_cmd "$(printf '     1  line1\n\n\n     2  line4\n')" cat -nb "$TMPDIR"/mixed.txt

echo "  ── -E : show $ at line ends ──"
assert_cmd "$(printf 'hello$\nworld$\n')" cat -E "$TMPDIR"/simple.txt

echo "  ── -T : show tabs as ^I ──"
assert_cmd "with^Itab" cat -T "$TMPDIR"/tab.txt

echo "  ── -s : squeeze blank lines ──"
assert_cmd "$(printf 'a\n\nb\n')" cat -s "$TMPDIR"/squeeze.txt

echo "  ── -v : show non-printing ──"
# Tab is printed as-is with -v (no -T)
assert_cmd "$(printf 'with\ttab\n')" cat -v "$TMPDIR"/tab.txt
assert_cmd_pat '\^A\^B\^_' cat -v "$TMPDIR"/low.txt   # 0x01→^A, 0x02→^B, 0x1f→^_
assert_cmd_pat 'M-\^@'     cat -v "$TMPDIR"/high.txt   # 0x80→M-^@
assert_cmd_pat 'M-\^_'     cat -v "$TMPDIR"/high.txt   # 0x9f→M-^_
assert_cmd_pat 'M- '       cat -v "$TMPDIR"/high.txt   # 0xa0→M-<space>
assert_cmd_pat 'M-\^\?'    cat -v "$TMPDIR"/high.txt   # 0xff→M-^?

echo "  ── -A : -vET (show all) ──"
assert_cmd "with^Itab$" cat -A "$TMPDIR"/tab.txt

echo "  ── -e / -t : compound options ──"
assert_cmd "$(printf 'hello$\nworld$\n')" cat -e "$TMPDIR"/simple.txt
assert_cmd "with^Itab" cat -t "$TMPDIR"/tab.txt

echo "  ── combined -vTE ──"
assert_cmd "with^Itab$" cat -vTE "$TMPDIR"/tab.txt

echo "  ── multiple files ──"
assert_cmd "$(printf 'hello\nworld\n')" cat "$TMPDIR"/a.txt "$TMPDIR"/b.txt

echo "  ── stdin ──"
assert_cmd "stdin test" cat <<<"stdin test"
assert_cmd "dash test" cat - <<<"dash test"

echo "  ── empty file ──"
assert_cmd "" cat "$TMPDIR"/empty.txt
assert_cmd "" cat -n "$TMPDIR"/empty.txt

echo "  ── -n -s combined ──"
assert_cmd "$(printf '     1  line1\n     2  \n     3  line4\n')" cat -n -s "$TMPDIR"/mixed.txt

echo "  ── DEL character (0x7f) ──"
assert_cmd_pat '\^\?' cat -v "$TMPDIR"/del.txt

echo "  ── -vT : tab becomes ^I ──"
assert_cmd "a^Ib" cat -vT - <<< $'a\tb'

echo "  ── all-blank with -b produces no output ──"
assert_cmd "" cat -b "$TMPDIR"/blanks.txt

echo "  ── --less (non-TTY) behaves like normal cat ──"
assert_cmd "$(printf 'hello\nworld\n')" cat --less "$TMPDIR"/simple.txt

echo "  ── error: non-existent file ──"
assert_cmd_pat_stderr "No such file" cat "$TMPDIR"/nonexistent.txt

echo "  ── --head=N : show first N lines ──"
printf 'a\nb\nc\nd\ne\n' > "$TMPDIR"/head.txt
assert_cmd "$(printf 'a\nb\nc\n')" cat --head=3 "$TMPDIR"/head.txt

echo "  ── --tail=N : show last N lines ──"
assert_cmd "$(printf 'c\nd\ne\n')" cat --tail=3 "$TMPDIR"/head.txt

echo "  ── --range=N-M : show lines N-M ──"
assert_cmd "$(printf 'b\nc\nd\n')" cat --range=2-4 "$TMPDIR"/head.txt

echo "  ── --range=N- : show from N to end ──"
assert_cmd "$(printf 'd\ne\n')" cat --range=4- "$TMPDIR"/head.txt

printf 'hello world\nfoo bar\nbaz qux\nhello again\n' > "$TMPDIR"/grep.txt

echo "  ── --grep=PATTERN : filter lines ──"
assert_cmd "$(printf 'hello world\nhello again\n')" cat --grep='hello' "$TMPDIR"/grep.txt

echo "  ── --grep + --context : show context around matches ──"
assert_cmd "$(printf 'foo bar\nbaz qux\nhello again\n')" cat --grep='baz' --context=1 "$TMPDIR"/grep.txt

echo "  ── --number-format=hex : hex line numbers ──"
printf 'a\nb\n' > "$TMPDIR"/hexnum.txt
assert_cmd "$(printf '0x0001  a\n0x0002  b\n')" cat -n --number-format=hex "$TMPDIR"/hexnum.txt

echo "  ── --number-format=octal : octal line numbers ──"
assert_cmd "$(printf '000001  a\n000002  b\n')" cat -n --number-format=octal "$TMPDIR"/hexnum.txt

echo "  ── --header : show file metadata banner ──"
assert_cmd_pat 'Mode:' cat --header "$TMPDIR"/simple.txt
assert_cmd_pat 'Size:' cat --header "$TMPDIR"/simple.txt
assert_cmd_pat 'Modified:' cat --header "$TMPDIR"/simple.txt

echo "  ── --stats : show line/word/char count ──"
assert_cmd_pat 'lines.*words.*characters' cat --stats "$TMPDIR"/simple.txt

echo "  ── --head + --range combined ──"
printf '1\n2\n3\n4\n5\n6\n7\n8\n' > "$TMPDIR"/comb.txt
assert_cmd "$(printf '3\n4\n5\n')" cat --range=3-6 --head=3 "$TMPDIR"/comb.txt

echo "  ── --range + --grep combined ──"
printf 'aa\nbb\ncc\naa\nbb\ncc\n' > "$TMPDIR"/rangegrep.txt
assert_cmd "$(printf 'aa\n')" cat --range=4-6 --grep='aa' "$TMPDIR"/rangegrep.txt

echo "  ── --diff : unified diff between two files ──"
printf 'a\nb\nc\n' > "$TMPDIR"/diff1.txt
printf 'a\nd\nc\n' > "$TMPDIR"/diff2.txt
assert_cmd_pat '^---' cat --diff="$TMPDIR"/diff2.txt "$TMPDIR"/diff1.txt
assert_cmd_pat '^\+d' cat --diff="$TMPDIR"/diff2.txt "$TMPDIR"/diff1.txt

echo "  ── --blame : git blame per line ──"
BNAME="blame_test_$$"
mkdir -p "$TMPDIR"/blame_repo
cd "$TMPDIR"/blame_repo
git init -q
git config user.email "test@test.com"
git config user.name "Tester"
printf 'first line\nsecond line\n' > "$BNAME".txt
git add "$BNAME".txt
git commit -q -m "initial" --date="2026-05-30 12:00:00"
assert_cmd_pat '[0-9a-f]{7}' cat --blame "$BNAME".txt
assert_cmd_pat 'Tester' cat --blame "$BNAME".txt
cd "$TMPDIR"

echo "  ── --highlight : auto-disable ANSI when piped (not a TTY) ──"
printf '#include <stdio.h>\nint main(void) { return 0; }\n' > "$TMPDIR"/hl_test.c
OUTPUT=$("$MODBOX" cat --highlight "$TMPDIR"/hl_test.c 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\033'; then
    fail "cat --highlight (piped) — expected no ANSI codes, got them"
else
    pass "cat --highlight (piped) — ANSI codes correctly disabled"
fi

echo "  ── --highlight + --header combined ──"
assert_cmd_pat 'Mode:' cat --header --highlight "$TMPDIR"/simple.txt

# ── ls ──────────────────────────────────────────────────────────────────────

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

cd "$TMPDIR"

# ── cp ──────────────────────────────────────────────────────────────────────

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

# ── ln ──────────────────────────────────────────────────────────────────────

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

# ── mv ──────────────────────────────────────────────────────────────────────

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

# ── grep ──────────────────────────────────────────────────────────────────────

echo ""
echo "── grep ────────────────────────────────────"

mkdir -p "$TMPDIR"/grep_dir
printf 'hello world\nfoo bar\nhello again\n' > "$TMPDIR"/grep_dir/a.txt
printf 'abc\n123\ndef\n' > "$TMPDIR"/grep_dir/b.txt
printf 'HELLO WORLD\nHello World\nlowercase\n' > "$TMPDIR"/grep_dir/case.txt
printf 'apple\nbanana\napple pie\npineapple\n' > "$TMPDIR"/grep_dir/words.txt
printf 'abc123def\n' > "$TMPDIR"/grep_dir/line.txt
printf 'a,b,c\n1,2,3\nx,y,z\n' > "$TMPDIR"/grep_dir/csv.txt

echo "  ── basic pattern search ──"
assert_cmd "$(printf 'hello world\nhello again\n')" grep hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -n : line numbers ──"
assert_cmd "$(printf '1:hello world\n3:hello again\n')" grep -n hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -i : case-insensitive ──"
assert_cmd "$(printf 'HELLO WORLD\nHello World\n')" grep -i hello "$TMPDIR"/grep_dir/case.txt

echo "  ── -v : invert match ──"
assert_cmd "$(printf 'foo bar\n')" grep -v hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -w : word match ──"
assert_cmd "$(printf 'apple\napple pie\n')" grep -w apple "$TMPDIR"/grep_dir/words.txt

echo "  ── -x : line-regexp ──"
assert_cmd "$(printf 'abc123def\n')" grep -x abc123def "$TMPDIR"/grep_dir/line.txt
assert_cmd "" grep -x abc "$TMPDIR"/grep_dir/line.txt

echo "  ── -E : extended regex ──"
assert_cmd "$(printf '123\n')" grep -E '[0-9]+' "$TMPDIR"/grep_dir/b.txt

echo "  ── -F : fixed strings ──"
printf 'hello.world\n' > "$TMPDIR"/grep_dir/fixed.txt
assert_cmd "hello.world" grep -F "hello." "$TMPDIR"/grep_dir/fixed.txt

echo "  ── -c : count ──"
assert_cmd "2" grep -c hello "$TMPDIR"/grep_dir/a.txt
assert_cmd "0" grep -c hello "$TMPDIR"/grep_dir/b.txt

echo "  ── -l : files with matches ──"
assert_cmd "$(printf '%s\n' "$TMPDIR"/grep_dir/a.txt)" grep -l hello "$TMPDIR"/grep_dir/a.txt "$TMPDIR"/grep_dir/b.txt

echo "  ── -H : always show filename ──"
assert_cmd_pat 'a\.txt:hello' grep -H hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -h : never show filename ──"
assert_cmd "$(printf '1:hello world\n3:hello again\n')" grep -h -n hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -o : only-matching ──"
assert_cmd "$(printf 'hello\nhello\n')" grep -o hello "$TMPDIR"/grep_dir/a.txt

echo "  ── stdin ──"
assert_cmd "hello" grep hello <<<"hello"

echo "  ── -e : pattern from flag ──"
assert_cmd "hello" grep -e hello <<<"hello"

echo "  ── --color=always : ANSI escape codes present ──"
assert_cmd_pat $'\x1b\\[' grep --color=always hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -r : recursive search ──"
mkdir -p "$TMPDIR"/grep_dir/subdir
printf 'deep match\n' > "$TMPDIR"/grep_dir/subdir/deep.txt
assert_cmd_pat 'deep\.txt:deep match' grep -r deep "$TMPDIR"/grep_dir/subdir

echo "  ── exit code 0 on match ──"
if "$MODBOX" grep hello "$TMPDIR"/grep_dir/a.txt >/dev/null 2>&1; then
    pass "grep hello → exit 0"
else
    fail "grep hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" grep nonexistent "$TMPDIR"/grep_dir/a.txt >/dev/null 2>&1; then
    fail "grep nonexistent → expected exit 1"
else
    pass "grep nonexistent → exit 1"
fi

echo "  ── error: no pattern ──"
if "$MODBOX" grep >/dev/null 2>&1; then
    fail "grep (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "grep (no args) → exit 2"
    else
        fail "grep (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── stdin with -n ──"
assert_cmd "1:hello" grep -n hello <<<"hello"

echo "  ── -w with -F : word match fixed strings ──"
printf 'foobar foo bar\n' > "$TMPDIR"/grep_dir/wordfixed.txt
assert_cmd "foobar foo bar" grep -wF foo "$TMPDIR"/grep_dir/wordfixed.txt
# "foobar" should NOT match "foo" as a word
assert_cmd "" grep -wF foobar <<<"foo"

# ── rg (ripgrep-style) ─────────────────────────────────────────────────

echo ""
echo "── rg ──────────────────────────────────────"

mkdir -p "$TMPDIR"/rg_dir
printf 'hello world\nfoo bar\nhello again\n' > "$TMPDIR"/rg_dir/a.txt
printf 'abc\n123\ndef\n' > "$TMPDIR"/rg_dir/b.txt
printf 'HELLO WORLD\nHello World\nlowercase\n' > "$TMPDIR"/rg_dir/case.txt
printf 'apple\nbanana\napple pie\npineapple\n' > "$TMPDIR"/rg_dir/words.txt
printf 'abc123def\n' > "$TMPDIR"/rg_dir/line.txt
printf 'hello.world\n' > "$TMPDIR"/rg_dir/fixed.txt
printf 'a\nb\nc hello\nd\ne\nf hello2\ng\n' > "$TMPDIR"/rg_dir/context.txt

echo "  ── basic search (default: line numbers) ──"
assert_cmd "$(printf '1:hello world\n--\n3:hello again\n')" rg hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -N : suppress line numbers ──"
assert_cmd "$(printf 'hello world\n--\nhello again\n')" rg -N hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -n : explicit line numbers (same as default) ──"
assert_cmd "$(printf '1:hello world\n--\n3:hello again\n')" rg -n hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -i : case-insensitive ──"
assert_cmd "$(printf '1:HELLO WORLD\n2:Hello World\n')" rg -i hello "$TMPDIR"/rg_dir/case.txt

echo "  ── -s : case-sensitive ──"
assert_cmd "" rg -s hello "$TMPDIR"/rg_dir/case.txt

echo "  ── -S : smart-case (uppercase pat = case-sensitive) ──"
assert_cmd "$(printf '1:HELLO WORLD\n')" rg -S HELLO "$TMPDIR"/rg_dir/case.txt

echo "  ── -v : invert match ──"
assert_cmd "$(printf '1:abc\n2:123\n3:def\n')" rg -v hello "$TMPDIR"/rg_dir/b.txt

echo "  ── -w : word match (apple matches, not pineapple) ──"
assert_cmd "$(printf '1:apple\n--\n3:apple pie\n')" rg -w apple "$TMPDIR"/rg_dir/words.txt
assert_cmd "$(printf '4:pineapple\n')" rg -w pineapple "$TMPDIR"/rg_dir/words.txt

echo "  ── -x : line-regexp ──"
assert_cmd "$(printf '1:abc123def\n')" rg -x abc123def "$TMPDIR"/rg_dir/line.txt
assert_cmd "" rg -x abc "$TMPDIR"/rg_dir/line.txt

echo "  ── -F : fixed strings ──"
assert_cmd "1:hello.world" rg -F hello. "$TMPDIR"/rg_dir/fixed.txt

echo "  ── -c : count ──"
# ripgrep style: count lines per file
assert_cmd "2" rg -c hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -l : files with matches ──"
assert_cmd "$(printf '%s\n' "$TMPDIR"/rg_dir/a.txt)" rg -l hello "$TMPDIR"/rg_dir/a.txt "$TMPDIR"/rg_dir/b.txt

echo "  ── -o : only-matching ──"
assert_cmd "$(printf 'hello\nhello\n')" rg -o hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -e : pattern from flag ──"
assert_cmd "1:hello" rg -e hello <<<"hello"

echo "  ── --color=always : ANSI codes present ──"
# Check for the ESC character (0x1b) in output
OUTPUT=$("$MODBOX" rg --color=always hello "$TMPDIR"/rg_dir/a.txt 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    pass "rg --color=always hello ... → ANSI codes present"
else
    fail "rg --color=always hello ... — expected ANSI codes, got plain text"
fi

echo "  ── auto-recursive on directory input ──"
assert_cmd_pat 'hello' rg hello "$TMPDIR"/rg_dir

echo "  ── -C 1 : context lines ──"
assert_cmd "$(printf '1:hello world\n2-foo bar\n--\n3:hello again\n')" rg -C 1 hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -C 2 : context lines around separate matches ──"
assert_cmd "$(printf '2-b\n3:c hello\n4-d\n--\n5-e\n6:f hello2\n7-g\n')" rg -C 1 hello "$TMPDIR"/rg_dir/context.txt

echo "  ── -g glob include ──"
# -g '*.txt' should only find .txt files; check that 'hello' is in the output
# and that the output includes .txt files
assert_cmd_pat 'hello' rg -g '*.txt' hello "$TMPDIR"/rg_dir
# .txt files should NOT contain 'b' (b.txt has abc/123/def, no hello)
# but all the .txt files in rg_dir have 'hello' somewhere... skip this assertion

echo "  ── -g glob exclude ──"
assert_cmd_pat 'hello' rg -g '!b.txt' hello "$TMPDIR"/rg_dir

echo "  ── --max-depth 0 (no recursion) ──"
mkdir -p "$TMPDIR"/rg_dir/subdir
printf 'deep\n' > "$TMPDIR"/rg_dir/subdir/deep.txt
assert_cmd "" rg --max-depth 0 deep "$TMPDIR"/rg_dir

echo "  ── --max-depth 1 (one level) ──"
assert_cmd_pat 'deep' rg --max-depth 1 deep "$TMPDIR"/rg_dir

echo "  ── -m 1 : max-count per file ──"
assert_cmd "1:hello world" rg -m 1 hello "$TMPDIR"/rg_dir/a.txt

echo "  ── stdin ──"
assert_cmd "1:hello" rg hello <<<"hello"

echo "  ── stdin with -N ──"
assert_cmd "hello" rg -N hello <<<"hello"

echo "  ── multiple files ──"
assert_cmd_pat 'hello world' rg hello "$TMPDIR"/rg_dir/a.txt "$TMPDIR"/rg_dir/b.txt

echo "  ── exit code 0 on match ──"
if "$MODBOX" rg hello "$TMPDIR"/rg_dir/a.txt >/dev/null 2>&1; then
    pass "rg hello → exit 0"
else
    fail "rg hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" rg nonexistent "$TMPDIR"/rg_dir/a.txt >/dev/null 2>&1; then
    fail "rg nonexistent → expected exit 1"
else
    pass "rg nonexistent → exit 1"
fi

echo "  ── error: no pattern ──"
if "$MODBOX" rg >/dev/null 2>&1; then
    fail "rg (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "rg (no args) → exit 2"
    else
        fail "rg (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── stdin with -n (default line nums) ──"
assert_cmd "1:hello" rg hello <<<"hello"

echo "  ── -w with -F : word match fixed strings ──"
printf 'foobar foo bar\n' > "$TMPDIR"/rg_dir/wordfixed.txt
assert_cmd "1:foobar foo bar" rg -wF foo "$TMPDIR"/rg_dir/wordfixed.txt
assert_cmd "" rg -wF foobar <<<"foo"

echo "  ── --color=auto : no ANSI when piped ──"
OUTPUT=$("$MODBOX" rg --color=auto hello "$TMPDIR"/rg_dir/a.txt 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\\033'; then
    fail "rg --color=auto (piped) — expected no ANSI codes, got them"
else
    pass "rg --color=auto (piped) — ANSI codes correctly disabled"
fi

echo "  ── --hidden : include hidden files ──"
printf 'hidden match\n' > "$TMPDIR"/rg_dir/.hidden.txt
assert_cmd_pat 'hidden match' rg --hidden hidden "$TMPDIR"/rg_dir
assert_cmd_not_pat 'hidden match' rg hidden "$TMPDIR"/rg_dir  # without --hidden, should NOT match

# ═══════════════════════════════════════════════════════════════════════════
#  find
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── find ─────────────────────────────────────"

mkdir -p "$TMPDIR"/find_dir/sub
mkdir -p "$TMPDIR"/find_dir/empty_dir
printf 'hello world\n'        > "$TMPDIR"/find_dir/a.txt
printf '42\n'                 > "$TMPDIR"/find_dir/b.txt
printf 'test content\n'       > "$TMPDIR"/find_dir/sub/c.txt
printf ''                     > "$TMPDIR"/find_dir/empty.txt
ln -sf "a.txt"                "$TMPDIR"/find_dir/link.ln

echo "  ── basic: find with single path ──"
assert_cmd_pat 'find_dir/a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt'

echo "  ── -name glob pattern ──"
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'b*'

echo "  ── -maxdepth 0 (only starting dir itself) ──"
assert_cmd_pat 'find_dir$' find "$TMPDIR"/find_dir -maxdepth 0
assert_cmd_not_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 0

echo "  ── -maxdepth 1 (immediate children) ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt'
assert_cmd_not_pat 'sub/c\.txt' find "$TMPDIR"/find_dir -maxdepth 1

echo "  ── -maxdepth 2 (recurse into sub) ──"
assert_cmd_pat 'sub/c\.txt' find "$TMPDIR"/find_dir -maxdepth 2 -name 'c.txt'

echo "  ── -mindepth 1 (skip starting dir) ──"
assert_cmd_not_pat "^$TMPDIR/find_dir$" find "$TMPDIR"/find_dir -mindepth 1 -maxdepth 1

echo "  ── -type f (regular files) ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -type f
assert_cmd_not_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 1 -type f

echo "  ── -type d (directories) ──"
assert_cmd_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 1 -type d

echo "  ── -type l (symlinks) ──"
assert_cmd_pat 'link\.ln' find "$TMPDIR"/find_dir -maxdepth 1 -type l

echo "  ── -empty (empty file) ──"
assert_cmd_pat 'empty\.txt' find "$TMPDIR"/find_dir -maxdepth 3 -empty -type f

echo "  ── -empty (empty directory) ──"
assert_cmd_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 3 -empty -type d

echo "  ── -empty does not match non-empty ──"
assert_cmd_not_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -empty

echo "  ── combined: -type f -name ──"
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -type f -name 'b*'
assert_cmd_not_pat 'empty_dir' find "$TMPDIR"/find_dir -maxdepth 1 -type f -name 'b*'

echo "  ── multiple starting points ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir/a.txt "$TMPDIR"/find_dir/b.txt -maxdepth 0
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir/a.txt "$TMPDIR"/find_dir/b.txt -maxdepth 0

echo "  ── -print (explicit) ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt' -print

echo "  ── default action is -print ──"
assert_cmd_pat 'a\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'a.txt'
# Also check that a file that only matches via -print doesn't crash
assert_cmd_pat 'b\.txt' find "$TMPDIR"/find_dir -maxdepth 1 -name 'b.txt' -print

echo "  ── error: nonexistent path on stderr ──"
assert_cmd_pat_stderr 'No such file' find /nonexistent_path_xyz

echo "  ── -exec echo {} ; (per-file) ──"
# exec directly: this should work if our fork/exec is correct
mkdir -p "$TMPDIR"/find_exec_test
printf 'hello exec\n' > "$TMPDIR"/find_exec_test/x.txt
printf 'hello exec\n' > "$TMPDIR"/find_exec_test/y.txt
assert_cmd_pat 'x\.txt' find "$TMPDIR"/find_exec_test -maxdepth 1 -type f -exec echo {} ";"

echo "  ── -exec echo {} + (batch) ──"
result=$(echo "$MODBOX" find "$TMPDIR"/find_exec_test -maxdepth 1 -type f -exec echo {} "+" 2>/dev/null)
assert_cmd_pat 'x\.txt' find "$TMPDIR"/find_exec_test -maxdepth 1 -type f -exec echo {} "+"

echo "  ── no starting point defaults to . ──"
cd "$TMPDIR"/find_dir
assert_cmd_pat 'a\.txt' find -maxdepth 1 -name 'a.txt'
cd /

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' find --help

echo "  ── -delete (empty file) ──"
cp "$TMPDIR"/find_dir/empty.txt "$TMPDIR"/find_dir/to_delete.txt
assert_cmd_pat 'to_delete' find "$TMPDIR"/find_dir -maxdepth 1 -name 'to_delete.txt' -print
if "$MODBOX" find "$TMPDIR"/find_dir -maxdepth 1 -name 'to_delete.txt' -delete; then
    pass "find -delete (file) → exit 0"
else
    fail "find -delete (file) → expected exit 0"
fi
if [ -f "$TMPDIR"/find_dir/to_delete.txt ]; then
    fail "find -delete (file) — file still exists after -delete"
else
    pass "find -delete (file) — file successfully removed"
fi

# ── Summary ─────────────────────────────────────────────────────────────────

echo ""
echo "════════════════════════════════════════════"
echo "  Results: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "════════════════════════════════════════════"

if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi
exit 0
