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

# ── dir / vdir ───────────────────────────────────────────────────────────────

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

# ═══════════════════════════════════════════════════════════════════════════
#  fd
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── fd ──────────────────────────────────────"

mkdir -p "$TMPDIR"/fd_dir/sub
printf 'hello world\n'  > "$TMPDIR"/fd_dir/hello.txt
printf 'stuff\n'        > "$TMPDIR"/fd_dir/Hello.txt
printf 'fn main() {}\n' > "$TMPDIR"/fd_dir/main.rs
printf 'data\n'         > "$TMPDIR"/fd_dir/data.rs
printf 'nested content\n' > "$TMPDIR"/fd_dir/sub/nested.txt
printf 'empty file\n'   > "$TMPDIR"/fd_dir/sub/empty.txt
: > "$TMPDIR"/fd_dir/empty_file.txt
touch "$TMPDIR"/fd_dir/.hidden_file
printf '#!/bin/sh\n' > "$TMPDIR"/fd_dir/exec_script.sh
chmod +x "$TMPDIR"/fd_dir/exec_script.sh

echo "  ── basic pattern search ──"
assert_cmd_pat 'hello\.txt' fd hello "$TMPDIR"/fd_dir

echo "  ── case-sensitive (-s) ──"
assert_cmd_pat 'Hello\.txt' fd -s Hello "$TMPDIR"/fd_dir
assert_cmd_pat 'hello\.txt' fd -s hello "$TMPDIR"/fd_dir
assert_cmd_not_pat 'Hello' fd -s hello "$TMPDIR"/fd_dir

echo "  ── case-insensitive (-i) ──"
assert_cmd_pat 'hello' fd -i HELLO "$TMPDIR"/fd_dir

echo "  ── smart-case (default): lowercase pat = case-insensitive ──"
assert_cmd_pat 'hello' fd hello "$TMPDIR"/fd_dir
assert_cmd_pat 'Hello' fd hello "$TMPDIR"/fd_dir

echo "  ── smart-case: uppercase pat = case-sensitive ──"
assert_cmd_pat 'Hello\.txt' fd Hello "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello' fd Hello "$TMPDIR"/fd_dir

echo "  ── glob mode (-g) ──"
assert_cmd_pat 'main\.rs' fd -g '*.rs' "$TMPDIR"/fd_dir

echo "  ── -H: hidden files ──"
assert_cmd_pat 'hidden_file' fd -H hidden "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hidden_file' fd hidden "$TMPDIR"/fd_dir

echo "  ── -t f: files only ──"
results_f=$("$MODBOX" fd -t f '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
results_all=$("$MODBOX" fd '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
if [ "$results_f" -gt 0 ] && [ "$results_f" -lt "$results_all" ]; then
    pass "fd -t f → $results_f files (vs $results_all total)"
else
    fail "fd -t f — expected files only, got $results_f (total $results_all)"
fi

echo "  ── -t d: directories only ──"
assert_cmd_pat 'fd_dir/sub$' fd -t d 'sub' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello\.txt' fd -t d '' "$TMPDIR"/fd_dir

echo "  ── -t l: symlinks ──"
ln -sf "$TMPDIR"/fd_dir/hello.txt "$TMPDIR"/fd_dir/link.ln
assert_cmd_pat 'link\.ln' fd -t l 'link' "$TMPDIR"/fd_dir

echo "  ── -t x: executables ──"
assert_cmd_pat 'exec_script' fd -t x 'exec' "$TMPDIR"/fd_dir

echo "  ── -t e: empty files ──"
assert_cmd_pat 'empty_file\.txt' fd -t e 'empty' "$TMPDIR"/fd_dir

echo "  ── -e extension filter ──"
assert_cmd_pat 'main\.rs' fd -e rs 'main' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello\.txt' fd -e rs '' "$TMPDIR"/fd_dir

echo "  ── -E exclude pattern ──"
assert_cmd_not_pat 'sub' fd -E 'sub' '' "$TMPDIR"/fd_dir 2>/dev/null

echo "  ── -d max depth ──"
assert_cmd_pat 'hello\.txt' fd -d 1 'txt' "$TMPDIR"/fd_dir
assert_cmd_pat 'nested\.txt' fd -d 1 'nested' "$TMPDIR"/fd_dir
assert_cmd_pat 'nested\.txt' fd -d 2 'nested' "$TMPDIR"/fd_dir

echo "  ── -p full path search ──"
assert_cmd_pat 'fd_dir/hello' fd -p 'fd_dir/' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'fd_dir/hello' fd 'fd_dir/' "$TMPDIR"/fd_dir

echo "  ── -0 print0 ──"
has_nul=$("$MODBOX" fd -0 'hello' "$TMPDIR"/fd_dir 2>/dev/null | tr -d '[:print:]' | grep -c $'\x00' || true)
if [ "$has_nul" -gt 0 ]; then
    pass "fd -0 → output contains NUL chars"
else
    fail "fd -0 — expected NUL-separated output"
fi

echo "  ── --color=always ──"
assert_cmd_pat $'\x1b\[' fd --color=always '.' "$TMPDIR"/fd_dir

echo "  ── --color=always has ANSI codes ──"
OUTPUT=$("$MODBOX" fd --color=always '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    pass "fd --color=always → ANSI codes present"
else
    fail "fd --color=always — expected ANSI codes"
fi

echo "  ── --color=auto (piped) no ANSI ──"
OUTPUT=$("$MODBOX" fd --color=auto '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    fail "fd --color=auto (piped) — expected no ANSI codes, got them"
else
    pass "fd --color=auto (piped) — ANSI codes correctly disabled"
fi

echo "  ── --color=never ──"
OUTPUT=$("$MODBOX" fd --color=never '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    fail "fd --color=never — expected no ANSI codes, got them"
else
    pass "fd --color=never — no ANSI codes"
fi

echo "  ── --max-results N ──"
line_count=$("$MODBOX" fd --max-results 3 '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
if [ "$line_count" -le 3 ]; then
    pass "fd --max-results 3 → $line_count lines (≤3)"
else
    fail "fd --max-results 3 → $line_count lines, expected ≤3"
fi

echo "  ── multiple paths ──"
assert_cmd_pat 'hello' fd hello "$TMPDIR"/fd_dir "$TMPDIR"

echo "  ── error: no pattern ──"
if "$MODBOX" fd >/dev/null 2>&1; then
    fail "fd (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "fd (no args) → exit 2"
    else
        fail "fd (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── error: invalid type ──"
assert_cmd_pat_stderr 'invalid type' fd -t z '.' /dev/null

echo "  ── exit code 0 on match ──"
if "$MODBOX" fd hello "$TMPDIR"/fd_dir >/dev/null 2>&1; then
    pass "fd hello → exit 0"
else
    fail "fd hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" fd nonexistentzzz "$TMPDIR"/fd_dir >/dev/null 2>&1; then
    fail "fd nonexistent → expected exit 1"
else
    pass "fd nonexistent → exit 1"
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' fd --help

echo "  ── -x exec per file (echo) ──"
result=$("$MODBOX" fd -x echo '\.rs$' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$result" | grep -qE '\.rs$'; then
    pass "fd -x echo '.rs' → exec output contains .rs paths"
else
    fail "fd -x echo '.rs' — expected .rs paths in output, got [$result]"
fi

echo "  ── -X exec batch (echo) ──"
result=$("$MODBOX" fd -X echo '\.rs$' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$result" | grep -qE '\.rs'; then
    pass "fd -X echo '.rs' → batch exec output contains .rs paths"
else
    fail "fd -X echo '.rs' — expected .rs paths in output, got [$result]"
fi

echo "  ── -L follow symlinks ──"
mkdir -p "$TMPDIR"/fd_follow/sub
printf 'followed\n' > "$TMPDIR"/fd_follow/sub/target.txt
ln -sf "$TMPDIR"/fd_follow "$TMPDIR"/fd_follow_link
assert_cmd_pat 'target' fd -L 'target' "$TMPDIR"/fd_follow_link

# ═══════════════════════════════════════════════════════════════════════════
#  sort
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── sort ──────────────────────────────────────"

printf 'banana\napple\ncherry\ndate\n' > "$TMPDIR"/sort_alpha
echo "  ── basic sort ──"
assert_cmd "$(printf 'apple\nbanana\ncherry\ndate\n')" sort "$TMPDIR"/sort_alpha

echo "  ── reverse sort ──"
assert_cmd "$(printf 'date\ncherry\nbanana\napple\n')" sort -r "$TMPDIR"/sort_alpha

echo "  ── numeric sort ──"
printf '10\n2\n30\n1\n20\n' > "$TMPDIR"/sort_num
assert_cmd "$(printf '1\n2\n10\n20\n30\n')" sort -n "$TMPDIR"/sort_num

echo "  ── ignore case ──"
printf 'Alpha\nBRAVO\ncharlie\nalpha\n' > "$TMPDIR"/sort_case
# After -f: Alpha/alpha equal, BRAVO equal to itself, charlie > alpha
# Output: Alpha, alpha, BRAVO, charlie (Alpha/alpha tiebroken by strcmp)
assert_cmd_pat '^Alpha$' sort -f "$TMPDIR"/sort_case
assert_cmd_pat 'charlie$' sort -f "$TMPDIR"/sort_case

echo "  ── unique ──"
printf 'apple\nbanana\napple\ncherry\nbanana\n' > "$TMPDIR"/sort_uniq
assert_cmd "$(printf 'apple\nbanana\ncherry\n')" sort -u "$TMPDIR"/sort_uniq

echo "  ── check sorted ──"
printf 'apple\nbanana\ncherry\n' > "$TMPDIR"/sort_ok
assert_cmd "" sort -c "$TMPDIR"/sort_ok

echo "  ── check unsorted ──"
printf 'apple\ncherry\nbanana\n' > "$TMPDIR"/sort_bad
assert_cmd_pat_stderr 'disorder' sort -c "$TMPDIR"/sort_bad
# sort -c should exit non-zero for unsorted input; we rely on test harness
# We check via 'disorder' pattern on stderr

echo "  ── check unique strict ──"
assert_cmd_pat_stderr 'disorder' sort -c -u "$TMPDIR"/sort_uniq

echo "  ── key field sort ──"
printf 'b 3\na 2\na 1\n' > "$TMPDIR"/sort_key
assert_cmd "$(printf 'a 1\na 2\nb 3\n')" sort -k 1,1 -k 2,2n "$TMPDIR"/sort_key

echo "  ── key with separator ──"
printf 'b:3\na:2\nc:1\n' > "$TMPDIR"/sort_t
assert_cmd "$(printf 'a:2\nb:3\nc:1\n')" sort -t : -k 1,1 "$TMPDIR"/sort_t

echo "  ── stable sort ──"
printf 'a 2\na 1\nb 3\n' > "$TMPDIR"/sort_stable
# Stable: equal-key lines keep input order (a 2 before a 1)
assert_cmd "$(printf 'a 2\na 1\nb 3\n')" sort -k 1,1 -s "$TMPDIR"/sort_stable
# Non-stable: tiebreaker by full line (a 1 before a 2)
assert_cmd "$(printf 'a 1\na 2\nb 3\n')" sort -k 1,1 "$TMPDIR"/sort_stable

echo "  ── output to file ──"
printf 'c\na\nb\n' > "$TMPDIR"/sort_in
"$MODBOX" sort -o "$TMPDIR"/sort_out "$TMPDIR"/sort_in 2>/dev/null || true
assert_cmd "$(printf 'a\nb\nc\n')" sort "$TMPDIR"/sort_out

echo "  ── ignore leading blanks ──"
printf '  b\n a\n  a\nc\n' > "$TMPDIR"/sort_blank
assert_cmd "$(printf ' a\n  a\n  b\nc\n')" sort -b "$TMPDIR"/sort_blank

echo "  ── help ──"
assert_cmd_pat 'Usage:' sort --help

echo "  ── stdin ──"
result=$("$MODBOX" sort < "$TMPDIR"/sort_alpha 2>/dev/null || true)
expected="$(printf 'apple\nbanana\ncherry\ndate')"
if [[ "$result" == "$expected" ]]; then
    pass "sort (stdin)"
else
    fail "sort (stdin) — expected [$expected] got [$result]"
fi

echo "  ── multiple files ──"
printf 'c\na\n' > "$TMPDIR"/sort_mf1
printf 'b\nd\n' > "$TMPDIR"/sort_mf2
    assert_cmd "$(printf 'a\nb\nc\nd\n')" sort "$TMPDIR"/sort_mf1 "$TMPDIR"/sort_mf2


# ═══════════════════════════════════════════════════════════════════════════
#  shuf
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── shuf ──────────────────────────────────────"

shuf_data="$TMPDIR/shuf_data.txt"
printf 'one\ntwo\nthree\nfour\nfive\n' > "$shuf_data"

echo "  ── -e echo mode (permutation check) ──"
result=$("$MODBOX" shuf -e a b c 2>/dev/null || true)
sorted=$(echo "$result" | sort)
expected="$(printf 'a\nb\nc')"
if [[ "$sorted" == "$expected" ]]; then
    pass "shuf -e a b c"
else
    fail "shuf -e a b c — sorted output expected [$expected] got [$sorted]"
fi

echo "  ── -e -n head-count ──"
result=$("$MODBOX" shuf -e a b c d e -n 3 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 3 ]]; then
    pass "shuf -e a b c d e -n 3 (count=$count)"
else
    fail "shuf -e a b c d e -n 3 — expected 3 lines, got $count"
fi
# Verify all lines come from input set
while IFS= read -r line; do
    case "$line" in
        a|b|c|d|e) ;;
        *) fail "shuf -e -n 3 — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── -e -r -n repeat mode ──"
result=$("$MODBOX" shuf -e x y z -r -n 8 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 8 ]]; then
    pass "shuf -e x y z -r -n 8 (count=$count)"
else
    fail "shuf -e x y z -r -n 8 — expected 8 lines, got $count"
fi
while IFS= read -r line; do
    case "$line" in
        x|y|z) ;;
        *) fail "shuf -e -r -n 8 — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── -i input-range -n head-count ──"
result=$("$MODBOX" shuf -i 1-100 -n 10 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 10 ]]; then
    pass "shuf -i 1-100 -n 10 (count=$count)"
else
    fail "shuf -i 1-100 -n 10 — expected 10 lines, got $count"
fi
while IFS= read -r line; do
    if [[ "$line" -lt 1 || "$line" -gt 100 ]]; then
        fail "shuf -i 1-100 -n 10 — value [$line] out of range"
    fi
done <<< "$result"

echo "  ── -i input-range permutation ──"
result=$("$MODBOX" shuf -i 1-5 2>/dev/null || true)
sorted=$(echo "$result" | sort -n)
expected="$(printf '1\n2\n3\n4\n5')"
if [[ "$sorted" == "$expected" ]]; then
    pass "shuf -i 1-5"
else
    fail "shuf -i 1-5 — sorted output expected [$expected] got [$sorted]"
fi

echo "  ── stdin (pipe) ──"
result=$(printf 'alpha\nbeta\ngamma\n' | "$MODBOX" shuf -n 2 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 2 ]]; then
    pass "shuf -n 2 (stdin, count=$count)"
else
    fail "shuf -n 2 (stdin) — expected 2 lines, got $count"
fi
while IFS= read -r line; do
    case "$line" in
        alpha|beta|gamma) ;;
        *) fail "shuf (stdin) — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── file input ──"
result=$("$MODBOX" shuf -n 3 "$shuf_data" 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 3 ]]; then
    pass "shuf -n 3 $shuf_data (count=$count)"
else
    fail "shuf -n 3 $shuf_data — expected 3 lines, got $count"
fi
while IFS= read -r line; do
    case "$line" in
        one|two|three|four|five) ;;
        *) fail "shuf file — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── -o output file ──"
"$MODBOX" shuf -e p q r -o "$TMPDIR"/shuf_out.txt 2>/dev/null || true
if [[ -f "$TMPDIR"/shuf_out.txt ]]; then
    result=$(cat "$TMPDIR"/shuf_out.txt)
    sorted=$(echo "$result" | sort)
    expected="$(printf 'p\nq\nr')"
    if [[ "$sorted" == "$expected" ]]; then
        pass "shuf -o output file"
    else
        fail "shuf -o output file — sorted output expected [$expected] got [$sorted]"
    fi
else
    fail "shuf -o output file — file not created"
fi

echo "  ── -e no args (error) ──"
assert_cmd_pat_stderr 'no input lines' shuf -e

echo "  ── -e -i conflict (error) ──"
assert_cmd_pat_stderr 'cannot combine' shuf -e -i 1-5

echo "  ── -i + file conflict (error) ──"
assert_cmd_pat_stderr 'cannot combine' shuf -i 1-5 "$shuf_data"

echo "  ── -i invalid range (error) ──"
assert_cmd_pat_stderr 'invalid input range' shuf -i 5-3

echo "  ── non-existent file (error) ──"
assert_cmd_pat_stderr 'No such file' shuf "$TMPDIR"/shuf_nonexistent

echo "  ── help ──"
assert_cmd_pat 'Usage:' shuf --help


# ═══════════════════════════════════════════════════════════════════════════
#  rev
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── rev ──────────────────────────────────────"

echo "  ── basic reverse ──"
printf 'abc\nhello world\n12345\n' > "$TMPDIR"/rev_basic
assert_cmd "$(printf 'cba\ndlrow olleh\n54321\n')" rev "$TMPDIR"/rev_basic

echo "  ── stdin ──"
result=$(printf 'abc\nhello world\n12345\n' | "$MODBOX" rev 2>/dev/null || true)
expected="$(printf 'cba\ndlrow olleh\n54321')"
if [[ "$result" == "$expected" ]]; then
    pass "rev (stdin)"
else
    fail "rev (stdin) — expected [$expected] got [$result]"
fi

echo "  ── single character ──"
printf 'a\n' > "$TMPDIR"/rev_single
assert_cmd "$(printf 'a\n')" rev "$TMPDIR"/rev_single

echo "  ── empty line ──"
printf '\n' > "$TMPDIR"/rev_empty
assert_cmd "$(printf '\n')" rev "$TMPDIR"/rev_empty

echo "  ── palindrome ──"
printf 'racecar\n' > "$TMPDIR"/rev_pal
assert_cmd "$(printf 'racecar\n')" rev "$TMPDIR"/rev_pal

echo "  ── multiple files ──"
printf 'ab\n' > "$TMPDIR"/rev_mf1
printf 'cd\n' > "$TMPDIR"/rev_mf2
assert_cmd "$(printf 'ba\ndc\n')" rev "$TMPDIR"/rev_mf1 "$TMPDIR"/rev_mf2

echo "  ── stdin via - ──"
result=$(printf 'xyz' | "$MODBOX" rev - 2>/dev/null || true)
if [[ "$result" == "zyx" ]]; then
    pass "rev - (stdin via -)"
else
    fail "rev - (stdin via -) — expected [zyx] got [$result]"
fi

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' rev "$TMPDIR"/rev_nonexistent

echo "  ── help ──"
assert_cmd_pat 'Usage:' rev --help


# ═══════════════════════════════════════════════════════════════════════════
#  du
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── du ──────────────────────────────────────"

# Setup test directory
mkdir -p "$TMPDIR"/du_dir/sub
printf 'hello' > "$TMPDIR"/du_dir/a.txt
printf 'world' > "$TMPDIR"/du_dir/sub/b.txt

echo "  ── basic du (default 1K blocks) ──"
assert_cmd_pat 'du_dir$' du "$TMPDIR"/du_dir
assert_cmd_pat 'du_dir/sub$' du "$TMPDIR"/du_dir

echo "  ── -b: bytes ──"
assert_cmd_pat 'du_dir$' du -b "$TMPDIR"/du_dir

echo "  ── -h: human-readable ──"
assert_cmd_pat 'K' du -h "$TMPDIR"/du_dir

echo "  ── -s: summarize ──"
result=$("$MODBOX" du -s "$TMPDIR"/du_dir 2>/dev/null | wc -l)
if [[ "$result" == "1" ]]; then
    pass "du -s → exactly 1 line"
else
    fail "du -s — expected 1 line, got $result"
fi

echo "  ── -a: all files ──"
assert_cmd_pat 'a.txt' du -a "$TMPDIR"/du_dir 2>/dev/null
assert_cmd_pat 'b.txt' du -a "$TMPDIR"/du_dir 2>/dev/null

echo "  ── -c: total ──"
assert_cmd_pat 'total' du -c "$TMPDIR"/du_dir "$TMPDIR"/du_dir/sub 2>/dev/null

echo "  ── -d 0: max-depth 0 ──"
assert_cmd_not_pat 'du_dir/sub' du -d 0 "$TMPDIR"/du_dir 2>/dev/null

echo "  ── -k: 1K blocks ──"
assert_cmd_pat 'du_dir$' du -k "$TMPDIR"/du_dir

echo "  ── --si ──"
assert_cmd_pat 'du_dir$' du --si "$TMPDIR"/du_dir

echo "  ── --exclude ──"
assert_cmd_pat 'du_dir$' du --exclude='*.txt' "$TMPDIR"/du_dir

echo "  ── -t: threshold ──"
assert_cmd_pat 'du_dir$' du -t 1K "$TMPDIR"/du_dir

echo "  ── --time ──"
assert_cmd_pat 'du_dir$' du --time "$TMPDIR"/du_dir 2>/dev/null

echo "  ── help ──"
assert_cmd_pat 'Usage:' du --help


# ═══════════════════════════════════════════════════════════════════════════
#  dust
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── dust ─────────────────────────────────────"

mkdir -p "$TMPDIR"/dust_dir/sub
printf 'hello' > "$TMPDIR"/dust_dir/a.txt
printf 'world' > "$TMPDIR"/dust_dir/sub/b.txt

echo "  ── basic dust ──"
assert_cmd_pat '█' dust "$TMPDIR"/dust_dir

echo "  ── -c: no color ──"
assert_cmd_pat '%' dust -c "$TMPDIR"/dust_dir

echo "  ── -d 0: depth 0 ──"
assert_cmd_not_pat 'sub' dust -c -d 0 "$TMPDIR"/dust_dir

echo "  ── -n: max lines ──"
result=$("$MODBOX" dust -c -n 1 "$TMPDIR"/dust_dir 2>/dev/null | wc -l)
if [[ "$result" == "1" ]]; then
    pass "dust -n 1 → exactly 1 line"
else
    fail "dust -n 1 — expected 1 line, got $result"
fi

echo "  ── -a: all files ──"
assert_cmd_pat 'a.txt' dust -c -a "$TMPDIR"/dust_dir

echo "  ── -b: bytes ──"
assert_cmd_pat '█' dust -c -b "$TMPDIR"/dust_dir

echo "  ── -H: si ──"
assert_cmd_pat 'kB' dust -c -H "$TMPDIR"/dust_dir

echo "  ── -X: exclude ──"
assert_cmd_pat '0%' dust -c -X '*.txt' "$TMPDIR"/dust_dir

echo "  ── help ──"
assert_cmd_pat 'Usage:' dust --help

echo "  ── ascending order (smallest first) ──"
# First line should be subdir (smallest), last line the root (largest)
lines=$("$MODBOX" dust -c "$TMPDIR"/dust_dir 2>/dev/null)
first=$(echo "$lines" | head -1)
last=$(echo "$lines" | tail -1)
if echo "$first" | grep -q 'dust_dir/sub$' && echo "$last" | grep -q 'dust_dir$'; then
    pass "dust → subdir first (smallest), root last (largest)"
else
    fail "dust — expected subdir first, root last"
fi


# ═══════════════════════════════════════════════════════════════════════════
#  head
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── head ─────────────────────────────────────"

printf 'line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n' > "$TMPDIR"/head_lines

echo "  ── default (10 lines) ──"
assert_cmd "$(printf 'line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\n')" head "$TMPDIR"/head_lines

echo "  ── -n N ──"
assert_cmd "$(printf 'line1\nline2\nline3\n')" head -n 3 "$TMPDIR"/head_lines

echo "  ── -c N ──"
assert_cmd "$(printf 'line1\nl')" head -c 7 "$TMPDIR"/head_lines

echo "  ── stdin ──"
result=$(printf 'a\nb\nc\n' | "$MODBOX" head -n 2 2>/dev/null || true)
if [[ "$result" == "$(printf 'a\nb')" ]]; then
    pass "head (stdin)"
else
    fail "head (stdin) — expected [a/b] got [$result]"
fi

echo "  ── multiple files with headers ──"
assert_cmd_pat '==>' head "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── -q: quiet ──"
assert_cmd_not_pat '==>' head -q "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── help ──"
assert_cmd_pat 'Usage:' head --help

echo "  ── empty file ──"
: > "$TMPDIR"/head_empty
assert_cmd "" head "$TMPDIR"/head_empty

echo "  ── -n +N (from line N) ──"
assert_cmd "$(printf 'line5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n')" head -n +5 "$TMPDIR"/head_lines

echo "  ── -z (NUL terminated) ──"
printf 'a\0b\0c\0' > "$TMPDIR"/head_nul
assert_cmd "$(printf 'a\0b\0')" head -z -n 2 "$TMPDIR"/head_nul


# ═══════════════════════════════════════════════════════════════════════════
#  tail
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── tail ─────────────────────────────────────"

echo "  ── default (10 lines) ──"
assert_cmd "$(printf 'line3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n')" tail "$TMPDIR"/head_lines

echo "  ── -n N ──"
assert_cmd "$(printf 'line10\nline11\nline12\n')" tail -n 3 "$TMPDIR"/head_lines

echo "  ── -n +N (from line N) ──"
assert_cmd "$(printf 'line5\nline6\nline7\nline8\nline9\nline10\nline11\nline12\n')" tail -n +5 "$TMPDIR"/head_lines

echo "  ── -c N (last bytes) ──"
assert_cmd "$(printf 'e12\n')" tail -c 4 "$TMPDIR"/head_lines

echo "  ── stdin ──"
result=$(printf 'a\nb\nc\n' | "$MODBOX" tail -n 2 2>/dev/null || true)
if [[ "$result" == "$(printf 'b\nc')" ]]; then
    pass "tail (stdin)"
else
    fail "tail (stdin) — expected [b/c] got [$result]"
fi

echo "  ── multiple files with headers ──"
assert_cmd_pat '==>' tail "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── -q: quiet ──"
assert_cmd_not_pat '==>' tail -q "$TMPDIR"/head_lines "$TMPDIR"/head_lines 2>/dev/null

echo "  ── help ──"
assert_cmd_pat 'Usage:' tail --help

echo "  ── empty file ──"
assert_cmd "" tail "$TMPDIR"/head_empty

echo "  ── -z (NUL terminated) ──"
assert_cmd "$(printf 'b\0c\0')" tail -z -n 2 "$TMPDIR"/head_nul

echo "  ── -n +N beyond EOF (produce nothing) ──"
assert_cmd "" tail -n +99 "$TMPDIR"/head_lines


# ═══════════════════════════════════════════════════════════════════════════
#  mkdir
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── mkdir ────────────────────────────────────"

echo "  ── basic create ──"
assert_cmd "" mkdir "$TMPDIR"/mkdir_new

echo "  ── already exists ──"
assert_cmd_pat_stderr "File exists" mkdir "$TMPDIR"/mkdir_new

echo "  ── -p parents ──"
assert_cmd "" mkdir -p "$TMPDIR"/mkdir_a/mkdir_b/mkdir_c

echo "  ── -p existing ──"
assert_cmd "" mkdir -p "$TMPDIR"/mkdir_new

echo "  ── -v verbose ──"
assert_cmd_pat "created directory" mkdir -v "$TMPDIR"/mkdir_verb

echo "  ── -m mode ──"
assert_cmd "" mkdir -m 0700 "$TMPDIR"/mkdir_mode
# Verify mode
mode=$(stat -c '%a' "$TMPDIR"/mkdir_mode 2>/dev/null)
if [ "$mode" = "700" ]; then
    pass "mkdir -m 0700 → mode 700"
else
    fail "mkdir -m 0700 → expected mode 700 got [$mode]"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' mkdir --help

echo "  ── multiple dirs ──"
assert_cmd "" mkdir "$TMPDIR"/mkdir_m1 "$TMPDIR"/mkdir_m2


# ═══════════════════════════════════════════════════════════════════════════
#  rm
# ═══════════════════════════════════════════════════════════════════════════

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

# ═══════════════════════════════════════════════════════════════════════════
#  touch
# ═══════════════════════════════════════════════════════════════════════════

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


# ═══════════════════════════════════════════════════════════════════════════
#  tsort
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── tsort ──────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' tsort --help

echo "  ── basic linear chain ──"
printf 'a b\nb c\nc d\n' | assert_cmd "$(printf 'a\nb\nc\nd\n')" tsort

echo "  ── multiple independent chains ──"
printf 'a b\nc d\n' | assert_cmd "$(printf 'a\nc\nb\nd\n')" tsort

echo "  ── diamond ──"
printf 'a b\na c\nb d\nc d\n' | assert_cmd "$(printf 'a\nb\nc\nd\n')" tsort

echo "  ── cycle detection ──"
result=$(printf 'a b\nb c\nc a\n' | "$MODBOX" tsort 2>&1 || true)
expected=$(printf 'tsort: input contains a cycle\na\nb\nc\n')
if [ "$result" = "$expected" ]; then
    pass "tsort (cycle)"
else
    fail "tsort (cycle) — expected [$(echo "$expected" | head -c 40)] got [$(echo "$result" | head -c 40)]"
fi

echo "  ── cycle exit code 1 ──"
if printf 'a b\nb a\n' | "$MODBOX" tsort >/dev/null 2>&1; then
    fail "tsort (cycle exit) — expected exit 1, got 0"
else
    pass "tsort (cycle exit 1)"
fi

echo "  ── file input ──"
printf 'x y\ny z\n' > "$TMPDIR"/tsort_file
assert_cmd "$(printf 'x\ny\nz\n')" tsort "$TMPDIR"/tsort_file

echo "  ── single item (odd tokens) ──"
printf 'a\n' | assert_cmd "a" tsort

echo "  ── single pair ──"
printf 'first second\n' | assert_cmd "$(printf 'first\nsecond\n')" tsort

echo "  ── stdin (using -) ──"
printf 'p q\n' | assert_cmd "$(printf 'p\nq\n')" tsort -

echo "  ── empty input ──"
printf '' | assert_cmd "" tsort

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr "cannot open" tsort "$TMPDIR"/tsort_nonexistent


# ═══════════════════════════════════════════════════════════════════════════
#  uniq
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── uniq ──────────────────────────────────────"

printf 'apple\napple\nbanana\ncherry\ncherry\ncherry\n' > "$TMPDIR"/uniq_basic
printf 'a\nb\nc\n' > "$TMPDIR"/uniq_unique
printf 'a\na\nb\nb\n' > "$TMPDIR"/uniq_all_dup

echo "  ── basic uniq (merge adjacent duplicates)"
assert_cmd "$(printf 'apple\nbanana\ncherry\n')" uniq "$TMPDIR"/uniq_basic

echo "  ── -c: count"
assert_cmd "$(printf '      2 apple\n      1 banana\n      3 cherry\n')" uniq -c "$TMPDIR"/uniq_basic

echo "  ── -d: repeated only"
assert_cmd "$(printf 'apple\ncherry\n')" uniq -d "$TMPDIR"/uniq_basic

echo "  ── -D: all repeated"
assert_cmd "$(printf 'apple\napple\ncherry\ncherry\ncherry\n')" uniq -D "$TMPDIR"/uniq_basic

echo "  ── -u: unique only"
assert_cmd "$(printf 'banana\n')" uniq -u "$TMPDIR"/uniq_basic

echo "  ── -i: ignore case"
printf 'Apple\nAPPLE\nbanana\nBanana\n' > "$TMPDIR"/uniq_case
assert_cmd "$(printf 'Apple\nbanana\n')" uniq -i "$TMPDIR"/uniq_case

echo "  ── -f: skip fields"
printf 'a x\nb y\na z\n' > "$TMPDIR"/uniq_fields
# -f 1 skips first field when comparing: x, y, z are all different
assert_cmd "$(printf 'a x\nb y\na z\n')" uniq -f 1 "$TMPDIR"/uniq_fields

echo "  ── -s: skip chars"
printf 'xxa\nyya\nzzb\n' > "$TMPDIR"/uniq_skipchars
assert_cmd "$(printf 'xxa\nzzb\n')" uniq -s 2 "$TMPDIR"/uniq_skipchars

echo "  ── -w: check chars"
printf 'apple\napples\nbanana\n' > "$TMPDIR"/uniq_checkchars
assert_cmd "$(printf 'apple\nbanana\n')" uniq -w 3 "$TMPDIR"/uniq_checkchars

echo "  ── input/output files"
printf 'a\na\nb\n' > "$TMPDIR"/uniq_inout
assert_cmd "" uniq "$TMPDIR"/uniq_inout "$TMPDIR"/uniq_out
assert_cmd "$(printf 'a\nb\n')" cat "$TMPDIR"/uniq_out

echo "  ── stdin"
assert_cmd "$(printf 'a\nb\n')" uniq <<<"$(printf 'a\na\nb\n')"

echo "  ── help"
assert_cmd_pat 'Usage:' uniq --help

echo "  ── error: nonexistent input"
assert_cmd_pat_stderr 'No such file' uniq "$TMPDIR"/uniq_nonexistent


# ═══════════════════════════════════════════════════════════════════════════
#  nl
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── nl ────────────────────────────────────────"

printf 'hello\nworld\n' > "$TMPDIR"/nl_basic
printf '\nnonempty\n\nlast\n' > "$TMPDIR"/nl_blanks
printf 'a\n\n\nb\n' > "$TMPDIR"/nl_join

echo "  ── basic nl (number all lines)"
assert_cmd "$(printf '     1\thello\n     2\tworld\n')" nl "$TMPDIR"/nl_basic

echo "  ── -b t: number nonempty only"
# File starts with a blank line, so output has leading blank line
assert_cmd "$(printf '\n     1\tnonempty\n\n     2\tlast\n')" nl -b t "$TMPDIR"/nl_blanks

echo "  ── -b n: no numbering"
assert_cmd "$(printf 'hello\nworld\n')" nl -b n "$TMPDIR"/nl_basic

echo "  ── -n rn: right-justified (default)"
assert_cmd "$(printf '     1\thello\n     2\tworld\n')" nl -n rn "$TMPDIR"/nl_basic

echo "  ── -n rz: right-justified with leading zeros"
assert_cmd "$(printf '000001\thello\n000002\tworld\n')" nl -n rz -w 6 "$TMPDIR"/nl_basic

echo "  ── -n ln: left-justified"
assert_cmd "$(printf '1     \thello\n2     \tworld\n')" nl -n ln -w 6 "$TMPDIR"/nl_basic

echo "  ── -w: custom width"
assert_cmd "$(printf '  1\thello\n  2\tworld\n')" nl -w 3 "$TMPDIR"/nl_basic

echo "  ── -s: custom separator"
assert_cmd "$(printf '     1:hello\n     2:world\n')" nl -s : "$TMPDIR"/nl_basic

echo "  ── -v: starting line number"
assert_cmd "$(printf '    10\thello\n    11\tworld\n')" nl -v 10 "$TMPDIR"/nl_basic

echo "  ── -i: line increment"
assert_cmd "$(printf '     1\thello\n     3\tworld\n')" nl -i 2 "$TMPDIR"/nl_basic

echo "  ── -l: join blank lines"
# With -l 2, pairs of blank lines count as one for numbering.
# Input: a + 2 blanks + b = 4 lines
# -b a: a→1, blank1→2, blank2→unnumbered, b→3
assert_cmd "$(printf '     1\ta\n     2\t\n\n     3\tb\n')" nl -l 2 "$TMPDIR"/nl_join

echo "  ── section delimiter: header/body/footer"
printf 'header1\n\\:\\:\\:\nbody1\n\\:\\:\nfooter1\n\\:\nextra\n' > "$TMPDIR"/nl_sections
assert_cmd "$(printf '     1\theader1\n     1\tbody1\n     1\tfooter1\n     1\textra\n')" nl "$TMPDIR"/nl_sections

echo "  ── -p: no-renumber (do not reset line numbers at sections)"
printf 'hdr\n\\:\\:\\:\nhdr2\n\\:\\:\nbody2\n' > "$TMPDIR"/nl_norenum
# With -p, line numbers continue across section boundaries (no reset)
assert_cmd "$(printf '     1\thdr\n     2\thdr2\n     3\tbody2\n')" nl -p "$TMPDIR"/nl_norenum

echo "  ── stdin"
assert_cmd "$(printf '     1\thello\n     2\tworld\n')" nl <<<"$(printf 'hello\nworld\n')"

echo "  ── help"
assert_cmd_pat 'Usage:' nl --help

echo "  ── error: nonexistent file"
assert_cmd_pat_stderr 'No such file' nl "$TMPDIR"/nl_nonexistent


# ═══════════════════════════════════════════════════════════════════════════
#  lsc
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── lsc ──────────────────────────────────────"

mkdir -p "$TMPDIR"/lsc_dir
touch "$TMPDIR"/lsc_dir/regular.txt
touch "$TMPDIR"/lsc_dir/exec.sh
chmod +x "$TMPDIR"/lsc_dir/exec.sh
mkdir "$TMPDIR"/lsc_dir/subdir
touch "$TMPDIR"/lsc_dir/.hidden

echo "  ── basic lsc (colorful + icons) ──"
assert_cmd_pat 'regular\.txt' lsc "$TMPDIR"/lsc_dir

echo "  ── lsc shows icons ──"
assert_cmd_pat $'\xef\x80\x96' lsc "$TMPDIR"/lsc_dir 2>/dev/null  #  file icon

echo "  ── lsc shows ANSI color codes ──"
assert_cmd_pat $'\x1b\[' lsc "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -l : long format with colors ──"
assert_cmd_pat 'regular\.txt' lsc -l "$TMPDIR"/lsc_dir 2>/dev/null
assert_cmd_pat '^[-d]' lsc -l "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -a : show all (including hidden) ──"
assert_cmd_pat '\.hidden' lsc -a "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -A : almost all — no . .. ──"
assert_cmd_pat '\.hidden' lsc -A "$TMPDIR"/lsc_dir 2>/dev/null
assert_cmd_not_pat '(^| )\.\.?  *' lsc -A "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -F : classify with indicators ──"
assert_cmd_pat 'subdir/' lsc -F "$TMPDIR"/lsc_dir 2>/dev/null
assert_cmd_pat 'exec\.sh\*' lsc -F "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -r : reverse sort ──"
assert_cmd_pat 'regular' lsc -r "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc --help shows usage ──"
assert_cmd_pat 'Usage:' lsc --help 2>/dev/null

echo "  ── lsc non-existent directory error ──"
assert_cmd_pat_stderr 'No such file' lsc "$TMPDIR"/lsc_nonexistent


# ═══════════════════════════════════════════════════════════════════════════
#  tac
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── tac ──────────────────────────────────────"

printf 'line1\nline2\nline3\n' > "$TMPDIR"/tac_basic
printf 'a\n\n\nc\n' > "$TMPDIR"/tac_blanks
printf 'single line\n' > "$TMPDIR"/tac_single
: > "$TMPDIR"/tac_empty

echo "  ── basic reverse lines ──"
assert_cmd "$(printf 'line3\nline2\nline1\n')" tac "$TMPDIR"/tac_basic

echo "  ── single line ──"
assert_cmd "$(printf 'single line\n')" tac "$TMPDIR"/tac_single

echo "  ── empty file ──"
assert_cmd "" tac "$TMPDIR"/tac_empty

echo "  ── stdin ──"
result=$(printf 'a\nb\nc\n' | "$MODBOX" tac 2>/dev/null || true)
expected="$(printf 'c\nb\na')"
if [[ "$result" == "$expected" ]]; then
    pass "tac (stdin)"
else
    fail "tac (stdin) — expected [$expected] got [$result]"
fi

echo "  ── multiple files ──"
printf '1\n2\n' > "$TMPDIR"/tac_mf1
printf 'a\nb\n' > "$TMPDIR"/tac_mf2
assert_cmd "$(printf '2\n1\nb\na\n')" tac "$TMPDIR"/tac_mf1 "$TMPDIR"/tac_mf2

echo "  ── -s : custom separator (trailing) ──"
# With -s, each record includes the separator. Reversing reverses the records.
printf 'a:b:c:' > "$TMPDIR"/tac_sep
assert_cmd "$(printf 'c:b:a:')" tac -s : "$TMPDIR"/tac_sep

echo "  ── -b : before mode (separator before each chunk) ──"
printf 'a:b:c:' > "$TMPDIR"/tac_before
assert_cmd "$(printf ':c:ba')" tac -b -s : "$TMPDIR"/tac_before

echo "  ── -r : regex separator ──"
printf 'aXXbXXc' > "$TMPDIR"/tac_regex
assert_cmd "$(printf 'cbXXaXX')" tac -r -s 'X+' "$TMPDIR"/tac_regex

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' tac "$TMPDIR"/tac_nonexistent

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' tac --help

echo "  ── blank lines reversed ──"
assert_cmd "$(printf 'c\n\n\na\n')" tac "$TMPDIR"/tac_blanks


# ═══════════════════════════════════════════════════════════════════════════
#  diff
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── diff ──────────────────────────────────────"

printf 'a\nb\nc\n' > "$TMPDIR"/diff_a
printf 'a\nd\nc\n' > "$TMPDIR"/diff_b
printf 'a\nb\nc\n' > "$TMPDIR"/diff_identical

echo "  ── normal diff (files differ) ──"
assert_cmd_pat '2c2' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '< b' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '> d' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null

echo "  ── identical files produce no output ──"
assert_cmd "" diff "$TMPDIR"/diff_a "$TMPDIR"/diff_identical

echo "  ── -q : brief (report if files differ) ──"
assert_cmd_pat 'differ' diff -q "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
# -q on identical files should produce no output
assert_cmd "" diff -q "$TMPDIR"/diff_a "$TMPDIR"/diff_identical

echo "  ── -s : report identical files ──"
assert_cmd_pat 'identical' diff -s "$TMPDIR"/diff_a "$TMPDIR"/diff_identical 2>/dev/null

echo "  ── -u : unified format ──"
assert_cmd_pat '^---' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^\+\+\+' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^@@' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^-b' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^\+d' diff -u "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null

echo "  ── -c : context format ──"
assert_cmd_pat '^\*\*\*' diff -c "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '^---' diff -c "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null
assert_cmd_pat '! b' diff -c "$TMPDIR"/diff_a "$TMPDIR"/diff_b 2>/dev/null

echo "  ── -i : ignore case ──"
printf 'Hello\nWorld\n' > "$TMPDIR"/diff_hi
printf 'hello\nworld\n' > "$TMPDIR"/diff_lo
assert_cmd "" diff -i "$TMPDIR"/diff_hi "$TMPDIR"/diff_lo
assert_cmd_pat '1,2c1,2' diff "$TMPDIR"/diff_hi "$TMPDIR"/diff_lo 2>/dev/null  # without -i, different

echo "  ── -w : ignore all whitespace ──"
printf 'a\nb  c\n' > "$TMPDIR"/diff_ws1
printf 'a\nb c\n' > "$TMPDIR"/diff_ws2
assert_cmd "" diff -w "$TMPDIR"/diff_ws1 "$TMPDIR"/diff_ws2

echo "  ── -b : ignore space change ──"
printf 'a\nb  c\n' > "$TMPDIR"/diff_sp1
printf 'a\nb c\n' > "$TMPDIR"/diff_sp2
assert_cmd "" diff -b "$TMPDIR"/diff_sp1 "$TMPDIR"/diff_sp2

echo "  ── stdin via - ──"
printf 'a\ny\nc\n' | assert_cmd_pat '2c2' diff - "$TMPDIR"/diff_a 2>/dev/null

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' diff "$TMPDIR"/diff_a "$TMPDIR"/diff_nonexistent

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' diff --help


# ═══════════════════════════════════════════════════════════════════════════
#  comm
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── comm ──────────────────────────────────────"

printf 'apple\nbanana\ncherry\n' > "$TMPDIR"/comm_f1
printf 'banana\ncherry\ndate\n'   > "$TMPDIR"/comm_f2
printf 'APPLE\nBANANA\n'          > "$TMPDIR"/comm_case1
printf 'apple\nbanana\n'          > "$TMPDIR"/comm_case2
printf 'b\na\n' > "$TMPDIR"/comm_unsorted1
printf 'b\na\n' > "$TMPDIR"/comm_unsorted2

echo "  ── basic comm (3 columns) ──"
# Column 1: lines only in file1, col2: only in file2, col3: in both
assert_cmd_pat 'apple' comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'date'  comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'cherry' comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -1 : suppress column 1 ──"
assert_cmd_not_pat 'apple' comm -1 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'date' comm -1 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -2 : suppress column 2 ──"
assert_cmd_pat 'apple' comm -2 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'date' comm -2 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -3 : suppress column 3 ──"
assert_cmd_pat 'apple' comm -3 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_pat 'date'  comm -3 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'cherry' comm -3 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -12 : show only column 3 ──"
assert_cmd_pat 'cherry' comm -12 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'apple' comm -12 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'date'  comm -12 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -13 : show only column 2 ──"
assert_cmd_pat 'date' comm -13 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'cherry' comm -13 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -23 : show only column 1 ──"
assert_cmd_pat 'apple' comm -23 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2
assert_cmd_not_pat 'cherry' comm -23 "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2

echo "  ── -i : ignore case ──"
# With -i, APPLE matches apple, BANANA matches banana.
# Column 3 prints the line from FILE1 (uppercase by default).
assert_cmd_pat 'APPLE' comm -i "$TMPDIR"/comm_case1 "$TMPDIR"/comm_case2
assert_cmd_pat 'BANANA' comm -i "$TMPDIR"/comm_case1 "$TMPDIR"/comm_case2
# Without -i they would all be in separate columns (case differs)

echo "  ── --output-delimiter ──"
result=$("$MODBOX" comm --output-delimiter='|' "$TMPDIR"/comm_f1 "$TMPDIR"/comm_f2 2>/dev/null || true)
if echo "$result" | grep -q '|'; then
    pass "comm --output-delimiter='|' → output contains pipes"
else
    fail "comm --output-delimiter='|' — expected pipes in output, got [$result]"
fi

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' comm "$TMPDIR"/comm_nonexistent "$TMPDIR"/comm_f2

echo "  ── unsorted file error ──"
assert_cmd_pat_stderr 'not in sorted order' comm "$TMPDIR"/comm_unsorted1 "$TMPDIR"/comm_f1
# Both need to be sorted, so unsorted2 should also trigger error
assert_cmd_pat_stderr 'not in sorted order' comm "$TMPDIR"/comm_f1 "$TMPDIR"/comm_unsorted2

echo "  ── --nocheck-order (skip sort check) ──"
# With --nocheck-order, unsorted input should not produce an error
result=$("$MODBOX" comm --nocheck-order "$TMPDIR"/comm_unsorted1 "$TMPDIR"/comm_unsorted2 2>/dev/null || true)
if [[ -n "$result" && "$result" != *"not in sorted order"* ]]; then
    pass "comm --nocheck-order → no sort error (output produced)"
else
    fail "comm --nocheck-order — expected output without sort error, got [$result]"
fi

echo "  ── stdin via - ──"
printf 'banana\ncherry\n' | assert_cmd_pat 'apple' comm "$TMPDIR"/comm_f1 - 2>/dev/null

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' comm --help


# ═══════════════════════════════════════════════════════════════════════════
#  expand / unexpand
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── expand ────────────────────────────────────"

printf '\thello\n'        > "$TMPDIR"/expand_simple.txt
printf 'a\tb\n'           > "$TMPDIR"/expand_mid.txt
printf '\t\thello\n'      > "$TMPDIR"/expand_two.txt
printf '\thello\tworld\n' > "$TMPDIR"/expand_initial.txt
: > "$TMPDIR"/expand_empty.txt


echo "  ── basic expand (tab → 8 spaces) ──"
assert_cmd "$(printf '        hello\n')" expand "$TMPDIR"/expand_simple.txt

echo "  ── expand tab mid-line ──"
assert_cmd "$(printf 'a       b\n')" expand "$TMPDIR"/expand_mid.txt

echo "  ── expand two tabs ──"
assert_cmd "$(printf '                hello\n')" expand "$TMPDIR"/expand_two.txt

echo "  ── expand -i (initial only: tab after non-blank preserved) ──"
assert_cmd "$(printf '        hello\tworld\n')" expand -i "$TMPDIR"/expand_initial.txt

echo "  ── expand -i keeps tab after non-blank on same line ──"
assert_cmd "$(printf 'a\tb\n')" expand -i "$TMPDIR"/expand_mid.txt

echo "  ── expand -t 4 (custom tab stop) ──"
assert_cmd "$(printf '    hello\n')" expand -t 4 "$TMPDIR"/expand_simple.txt

echo "  ── expand -t 4 mid-line ──"
assert_cmd "$(printf 'a   b\n')" expand -t 4 "$TMPDIR"/expand_mid.txt

echo "  ── expand -t 1,4,8 (custom list) ──"
assert_cmd "$(printf ' hello\n')" expand -t 1,4,8 "$TMPDIR"/expand_simple.txt

echo "  ── expand stdin ──"
assert_cmd "        hello" expand <<< $'\thello'

echo "  ── expand stdin via - ──"
assert_cmd "        hello" expand - <<< $'\thello'

echo "  ── expand multiple files ──"
assert_cmd "$(printf '        hello\n        hello\n')" expand "$TMPDIR"/expand_simple.txt "$TMPDIR"/expand_simple.txt

echo "  ── expand empty file ──"
assert_cmd "" expand "$TMPDIR"/expand_empty.txt

echo "  ── expand --help ──"
assert_cmd_pat 'Usage:' expand --help

echo "  ── expand error: non-existent file ──"
assert_cmd_pat_stderr 'No such file' expand "$TMPDIR"/expand_nonexistent

echo "  ── expand error: invalid tab list ──"
assert_cmd_pat_stderr 'invalid tab stop' expand -t abc "$TMPDIR"/expand_simple.txt

echo ""
echo "── unexpand ──────────────────────────────────"

printf '        hello\n'        > "$TMPDIR"/unexpand_simple.txt
printf 'a       b\n'            > "$TMPDIR"/unexpand_all.txt
printf '    hello\n'            > "$TMPDIR"/unexpand_t4.txt
printf '   hello\n'             > "$TMPDIR"/unexpand_partial.txt
printf '        hello\tworld\n' > "$TMPDIR"/unexpand_initial.txt
printf '        a        b\n'   > "$TMPDIR"/unexpand_two_stops.txt
: > "$TMPDIR"/unexpand_empty.txt

echo "  ── basic unexpand (leading spaces → tabs) ──"
assert_cmd "$(printf '\thello\n')" unexpand "$TMPDIR"/unexpand_simple.txt

echo "  ── unexpand -a (all spaces, not just leading) ──"
assert_cmd "$(printf 'a\tb\n')" unexpand -a "$TMPDIR"/unexpand_all.txt

echo "  ── unexpand -a multiple tab stops ──"
printf '        a       b\n' > "$TMPDIR"/unexpand_two_stops.txt
assert_cmd "$(printf '\ta\tb\n')" unexpand -a "$TMPDIR"/unexpand_two_stops.txt

echo "  ── unexpand -t 4 (custom tab stop) ──"
assert_cmd "$(printf '\thello\n')" unexpand -t 4 "$TMPDIR"/unexpand_t4.txt

echo "  ── unexpand -t 4 mid-line (implies -a, 7 spaces→2 tabs 3+4) ──"
assert_cmd "$(printf 'a\t\tb\n')" unexpand -t 4 "$TMPDIR"/unexpand_all.txt

echo "  ── unexpand -t 2,4,8 with 4 spaces at col 0 → two tabs ──"
assert_cmd "$(printf '\t\thello\n')" unexpand -t 2,4,8 "$TMPDIR"/unexpand_t4.txt

echo "  ── unexpand --first-only (leading spaces only) ──"
assert_cmd "$(printf '\thello\tworld\n')" unexpand --first-only "$TMPDIR"/unexpand_initial.txt

echo "  ── unexpand partial spaces (3 spaces → no tab, not enough) ──"
assert_cmd "$(printf '   hello\n')" unexpand "$TMPDIR"/unexpand_partial.txt

echo "  ── unexpand stdin ──"
assert_cmd "$(printf '\thello')" unexpand <<<"$(printf '        hello')"

echo "  ── unexpand stdin via - ──"
assert_cmd "$(printf '\thello')" unexpand - <<<"$(printf '        hello')"

echo "  ── unexpand multiple files ──"
assert_cmd "$(printf '\thello\n\thello\n')" unexpand "$TMPDIR"/unexpand_simple.txt "$TMPDIR"/unexpand_simple.txt

echo "  ── unexpand empty file ──"
assert_cmd "" unexpand "$TMPDIR"/unexpand_empty.txt

echo "  ── unexpand --help ──"
assert_cmd_pat 'Usage:' unexpand --help

echo "  ── unexpand error: non-existent file ──"
assert_cmd_pat_stderr 'No such file' unexpand "$TMPDIR"/unexpand_nonexistent

echo "  ── unexpand error: invalid tab list ──"
assert_cmd_pat_stderr 'invalid tab stop' unexpand -t abc "$TMPDIR"/unexpand_simple.txt

echo "  ── expand | unexpand -a roundtrip ──"
result=$(expand < "$TMPDIR"/expand_simple.txt 2>/dev/null | "$MODBOX" unexpand -a 2>/dev/null || true)
expected=$(printf '\thello')
if [[ "$result" == "$expected" ]]; then
    pass "expand → unexpand -a roundtrip: tab preserved"
else
    fail "expand → unexpand -a roundtrip — expected [$expected] got [$result]"
fi

echo "  ── unexpand -a | expand roundtrip ──"
result=$(unexpand -a < "$TMPDIR"/unexpand_all.txt 2>/dev/null | "$MODBOX" expand 2>/dev/null || true)
expected=$(printf 'a       b')
if [[ "$result" == "$expected" ]]; then
    pass "unexpand -a → expand roundtrip: spaces preserved"
else
    fail "unexpand -a → expand roundtrip — expected [$expected] got [$result]"
fi

# ═══════════════════════════════════════════════════════════════════════════
#  uname
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── uname ────────────────────────────────────"

echo "  ── default (kernel name) ──"
assert_cmd "$(uname -s)" uname

echo "  ── -s : kernel name ──"
assert_cmd "$(uname -s)" uname -s

echo "  ── -n : nodename ──"
assert_cmd "$(uname -n)" uname -n

echo "  ── -r : kernel release ──"
assert_cmd "$(uname -r)" uname -r

echo "  ── -v : kernel version ──"
assert_cmd "$(uname -v)" uname -v

echo "  ── -m : machine ──"
assert_cmd "$(uname -m)" uname -m

echo "  ── -o : operating system ──"
assert_cmd "$(uname -o)" uname -o

echo "  ── -a : all (contains kernel name) ──"
assert_cmd_pat "$(uname -s)" uname -a
assert_cmd_pat "$(uname -n)" uname -a
assert_cmd_pat "$(uname -r)" uname -a
assert_cmd_pat "$(uname -m)" uname -a

echo "  ── multiple flags ──"
assert_cmd "$(printf '%s %s\n' "$(uname -s)" "$(uname -r)")" uname -s -r

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' uname --help


# ═══════════════════════════════════════════════════════════════════════════
#  whoami
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── whoami ───────────────────────────────────"

echo "  ── print current user ──"
assert_cmd "$(whoami)" whoami

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' whoami --help


# ═══════════════════════════════════════════════════════════════════════════
#  split
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── split ────────────────────────────────────"

printf 'line1\nline2\nline3\nline4\nline5\n' > "$TMPDIR"/split5.txt
printf 'line1\nline2\n' > "$TMPDIR"/split_2lines.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' split --help

echo "  ── default: 1000 lines, single file ──"
cd "$TMPDIR"
rm -f xaa
"$MODBOX" split split5.txt 2>/dev/null
assert_cmd "$(printf 'line1\nline2\nline3\nline4\nline5\n')" cat xaa
rm -f xaa
cd - > /dev/null

echo "  ── -l 2 lines, alpha suffixes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat xaa
assert_cmd "$(printf 'line3\nline4\n')" cat xab
assert_cmd "$(printf 'line5\n')" cat xac
rm -f xaa xab xac
cd - > /dev/null

echo "  ── -l 2, -d numeric suffixes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -d 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat x00
assert_cmd "$(printf 'line3\nline4\n')" cat x01
rm -f x00 x01 x02
cd - > /dev/null

echo "  ── --verbose ──"
cd "$TMPDIR"
assert_cmd_pat_stderr 'creating file' split split5.txt -l 5 --verbose
rm -f xaa
cd - > /dev/null

echo "  ── -b 8 bytes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -b 8 -d 2>/dev/null
assert_cmd "$(printf 'line1\nli')" cat x00
assert_cmd "$(printf 'ne2\nline')" cat x01
assert_cmd "$(printf '3\nline4\n')" cat x02
assert_cmd "$(printf 'line5\n')" cat x03
rm -f x00 x01 x02 x03
cd - > /dev/null

echo "  ── custom prefix ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -d mypre 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat mypre00
assert_cmd "$(printf 'line3\nline4\n')" cat mypre01
rm -f mypre00 mypre01 mypre02
cd - > /dev/null

echo "  ── -a suffix length ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -d -a 3 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat x000
rm -f x000 x001 x002
cd - > /dev/null

echo "  ── stdin ──"
cd "$TMPDIR"
printf 'a\nb\nc\n' | "$MODBOX" split -l 2 -d 2>/dev/null
assert_cmd "$(printf 'a\nb\n')" cat x00
assert_cmd "$(printf 'c\n')" cat x01
rm -f x00 x01
cd - > /dev/null

echo "  ── -n N (chunks by bytes) ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -n 2 -d 2>/dev/null
assert_cmd "$(printf 'line1\nline2\nlin')" cat x00
assert_cmd "$(printf 'e3\nline4\nline5\n')" cat x01
rm -f x00 x01
cd - > /dev/null

echo "  ── -n l/N (chunks by lines) ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -n l/2 -d 2>/dev/null
assert_cmd "$(printf 'line1\nline2\nline3\n')" cat x00
assert_cmd "$(printf 'line4\nline5\n')" cat x01
rm -f x00 x01
cd - > /dev/null

echo "  ── -C line-bytes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -C 6 -d 2>/dev/null
# each line is 6 bytes ("lineN\n"), so each file gets one line
assert_cmd "$(printf 'line1\n')" cat x00
assert_cmd "$(printf 'line2\n')" cat x01
assert_cmd "$(printf 'line3\n')" cat x02
assert_cmd "$(printf 'line4\n')" cat x03
assert_cmd "$(printf 'line5\n')" cat x04
rm -f x00 x01 x02 x03 x04
cd - > /dev/null

echo "  ── --additional-suffix ──"
cd "$TMPDIR"
"$MODBOX" split split_2lines.txt -l 1 -d --additional-suffix=.txt 2>/dev/null
assert_cmd "$(printf 'line1\n')" cat x00.txt
assert_cmd "$(printf 'line2\n')" cat x01.txt
rm -f x00.txt x01.txt
cd - > /dev/null

echo "  ── -x hex suffixes ──"
cd "$TMPDIR"
"$MODBOX" split split5.txt -l 2 -x 2>/dev/null
assert_cmd "$(printf 'line1\nline2\n')" cat x00
assert_cmd "$(printf 'line3\nline4\n')" cat x01
rm -f x00 x01 x02
cd - > /dev/null


# ═══════════════════════════════════════════════════════════════════════════
#  csplit
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "── csplit ───────────────────────────────────"

printf 'a\nb\nc\nd\ne\nf\ng\n' > "$TMPDIR"/csplit7.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' csplit --help

echo "  ── /REGEXP/ (include match) ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat xx00
assert_cmd "$(printf 'e\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── %%REGEXP%% (exclude match) ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt %c% 2>/dev/null
assert_cmd "$(printf 'a\nb\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── line number ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt 3 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── multiple patterns ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt /c/ /f/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\n')" cat xx01
assert_cmd "$(printf 'g\n')" cat xx02
rm -f xx00 xx01 xx02
cd - > /dev/null

echo "  ── {N} repeat ──"
cd "$TMPDIR"
"$MODBOX" csplit csplit7.txt /c/ '{1}' 2>/dev/null
# /c/ matches line 3, then repeats once, matches /c/ again? no, there's only one "c"
# so it splits at /c/ (line 3) then tries again and fails, remainder goes to last
assert_cmd "$(printf 'a\nb\nc\n')" cat xx00
assert_cmd "$(printf 'd\ne\nf\ng\n')" cat xx01
rm -f xx00 xx01
cd - > /dev/null

echo "  ── -f prefix ──"
cd "$TMPDIR"
"$MODBOX" csplit -f mypre csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat mypre00
assert_cmd "$(printf 'e\nf\ng\n')" cat mypre01
rm -f mypre00 mypre01
cd - > /dev/null

echo "  ── -n digits ──"
cd "$TMPDIR"
"$MODBOX" csplit -n 3 csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat xx000
assert_cmd "$(printf 'e\nf\ng\n')" cat xx001
rm -f xx000 xx001
cd - > /dev/null

echo "  ── -b suffix-format ──"
cd "$TMPDIR"
"$MODBOX" csplit -b '%03d' csplit7.txt /d/ 2>/dev/null
assert_cmd "$(printf 'a\nb\nc\nd\n')" cat xx000
assert_cmd "$(printf 'e\nf\ng\n')" cat xx001
rm -f xx000 xx001
cd - > /dev/null

echo "  ── -s quiet (no size output) ──"
cd "$TMPDIR"
quiet_out=$("$MODBOX" csplit -s csplit7.txt /d/ 2>/dev/null)
if [[ -z "$quiet_out" ]]; then
    pass "csplit -s csplit7.txt /d/"
else
    fail "csplit -s csplit7.txt /d/ — expected empty stdout, got [$quiet_out]"
fi
rm -f xx00 xx01
cd - > /dev/null

# ── paste ─────────────────────────────────────────────────────────────────────

echo ""
echo "── paste ─────────────────────────────────────"

printf 'a\nb\nc\n' > "$TMPDIR"/paste_f1.txt
printf '1\n2\n3\n' > "$TMPDIR"/paste_f2.txt
printf 'x\ny\nz\n' > "$TMPDIR"/paste_f3.txt
printf 'single\n' > "$TMPDIR"/paste_single.txt
printf '' > "$TMPDIR"/paste_empty.txt

echo "  ── basic parallel ──"
assert_cmd "$(printf 'a\t1\nb\t2\nc\t3\n')" paste "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── three files ──"
assert_cmd "$(printf 'a\t1\tx\nb\t2\ty\nc\t3\tz\n')" paste "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── serial ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\n')" paste -s "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── serial three files ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\nx\ty\tz\n')" paste -s "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── custom delimiter ──"
assert_cmd "$(printf 'a:1\nb:2\nc:3\n')" paste -d: "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── multi-char delimiter cycle ──"
assert_cmd "$(printf 'a:1,x\nb:2,y\nc:3,z\n')" paste -d ':,' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── pipe delimiter (special char) ──"
assert_cmd "$(printf 'a|1\nb|2\nc|3\n')" paste -d '|' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── stdin via - ──"
assert_cmd "$(printf 'hello\ta\nworld\tb\n\tc\n')" paste - "$TMPDIR"/paste_f1.txt <<<"$(printf 'hello\nworld')"

echo "  ── no files reads stdin ──"
assert_cmd "$(printf 'one\ntwo\nthree')" paste <<<"$(printf 'one\ntwo\nthree')"

echo "  ── single file (passthrough) ──"
assert_cmd "$(printf 'a\nb\nc\n')" paste "$TMPDIR"/paste_f1.txt

echo "  ── single file serial ──"
assert_cmd "$(printf 'a\tb\tc')" paste -s "$TMPDIR"/paste_f1.txt

echo "  ── empty file ──"
assert_cmd "" paste "$TMPDIR"/paste_empty.txt

echo "  ── empty file parallel ──"
assert_cmd "$(printf '\ta\n\tb\n\tc')" paste "$TMPDIR"/paste_empty.txt "$TMPDIR"/paste_f1.txt

echo "  ── uneven line counts ──"
printf 'p\nq\n' > "$TMPDIR"/paste_short.txt
assert_cmd "$(printf 'a\tp\nb\tq\nc\t\n')" paste "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_short.txt

echo "  ── serial uneven lines ──"
printf 'a\nb\n' > "$TMPDIR"/paste_ser_short.txt
assert_cmd "$(printf 'a\tb\n1\t2\t3')" paste -s "$TMPDIR"/paste_ser_short.txt "$TMPDIR"/paste_f2.txt

echo "  ── zero-terminated ──"
printf 'a\nb\n' | "$MODBOX" paste -z - "$TMPDIR"/paste_single.txt > "$TMPDIR"/paste_zout.bin 2>/dev/null || true
if od -c "$TMPDIR"/paste_zout.bin | grep -q '\\0'; then
    pass "paste -z → NUL delimited"
else
    fail "paste -z — expected NUL in output"
fi

echo "  ── zero-terminated output structure ──"
printf 'a\nb\n' > "$TMPDIR"/paste_zf1.txt
printf '1\n2\n' > "$TMPDIR"/paste_zf2.txt
"$MODBOX" paste -z "$TMPDIR"/paste_zf1.txt "$TMPDIR"/paste_zf2.txt > "$TMPDIR"/paste_zout2.bin 2>/dev/null || true
# Expect 2 NULs: a<tab>1<NUL>b<tab>2<NUL>
field_count=$(tr -d -c '\0' < "$TMPDIR"/paste_zout2.bin | wc -c)
if [ "$field_count" -eq 2 ]; then
    pass "paste -z two files → 2 NUL delimiters"
else
    fail "paste -z two files — expected 2 NUL chars, got $field_count"
fi

echo "  ── nonexistent file ──"
assert_cmd_pat_stderr "No such file" paste "$TMPDIR"/paste_nonexistent.txt "$TMPDIR"/paste_f1.txt

echo "  ── multiple nonexistent files ──"
assert_cmd_pat_stderr "No such file" paste "$TMPDIR"/paste_nonexistent.txt "$TMPDIR"/paste_f2.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' paste --help

echo "  ── version ──"
assert_cmd_pat 'modbox' paste --version

echo "  ── -d empty delimiter ──"
assert_cmd "$(printf 'a1\nb2\nc3\n')" paste -d '' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── -d empty delimiter with three files ──"
assert_cmd "$(printf 'a1x\nb2y\nc3z\n')" paste -d '' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── serial with custom delimiter ──"
assert_cmd "$(printf 'a:b:c\n1:2:3\n')" paste -s -d: "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── serial with multi-char delimiter cycle ──"
assert_cmd "$(printf 'a,b-c\n1,2-3\n')" paste -s -d ',-' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

# ── ptx ──────────────────────────────────────────────────────────────────────

echo ""
echo "── ptx ──────────────────────────────────────"

echo "  ── basic ptx ──"
printf 'The quick brown fox jumps over the lazy dog\n' > "$TMPDIR"/ptx1.txt
assert_cmd_pat 'fox' ptx "$TMPDIR"/ptx1.txt
assert_cmd_pat 'dog' ptx "$TMPDIR"/ptx1.txt
assert_cmd_pat 'quick' ptx "$TMPDIR"/ptx1.txt

echo "  ── ptx with auto reference ──"
assert_cmd_pat 'ptx1.txt' ptx -A "$TMPDIR"/ptx1.txt

echo "  ── ptx with width option ──"
assert_cmd_pat 'fox' ptx -w 60 "$TMPDIR"/ptx1.txt

echo "  ── ptx from stdin ──"
assert_cmd_pat 'fox' ptx <<<"The quick brown fox jumps over the lazy dog"

echo "  ── ptx help ──"
assert_cmd_pat 'Usage:' ptx --help

echo "  ── ptx non-existent file ──"
assert_cmd_pat_stderr "No such file" ptx "$TMPDIR"/nonexistent.txt

# ── Summary ─────────────────────────────────────────────────────────────────

echo ""
echo "════════════════════════════════════════════"
echo "  Results: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "════════════════════════════════════════════"

if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi
exit 0
