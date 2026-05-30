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

# ── Summary ─────────────────────────────────────────────────────────────────

echo ""
echo "════════════════════════════════════════════"
echo "  Results: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "════════════════════════════════════════════"

if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi
exit 0
