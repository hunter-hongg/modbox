#!/usr/bin/env bash
#
# test_namei.sh — modbox namei: path component analysis
#
# Exercises the public CLI seam of `modbox namei`. The fixture lives in the
# runner's TMPDIR under a one-character directory name, so the printed listings
# contain no host-dependent paths and every full output can be compared
# byte-exactly. Owner and group names are still host-dependent, so the
# normalize-then-compare helper below rewrites them to U/G first.
#
set -o nounset
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

UNAME="$(id -un)"
GNAME="$(id -gn)"

# assert_cmd_norm EXPECTED args... — runs modbox, rewrites the host user and
# group names to a single N placeholder, then compares stdout exactly. A single
# token is used because the owner and group names are often identical (e.g.
# "hunter"), which makes a two-token rewrite ambiguous.
assert_cmd_norm() {
    local expected="$1"; shift
    local actual
    actual=$("$MODBOX" "$@" 2>/dev/null | sed -E "s/${UNAME//./\\.}/N/g; s/${GNAME//./\\.}/N/g")
    if [[ "$actual" == "$expected" ]]; then
        pass "$*"
    else
        fail "$* — expected [$(echo "$expected" | tr '\n' '|')] got [$(echo "$actual" | tr '\n' '|')]"
    fi
}

# ── fixtures ────────────────────────────────────────────────────────────────
# The runner's cwd is the per-test TMPDIR; a relative fixture avoids the
# host-dependent $TMPDIR prefix (an absolute /tmp path would drag the /tmp
# mount point into every -x test).
F=ne
mkdir -p "$F/sub/deep/inner" "$F/nested"
echo payload > "$F/file.txt"
printf '' > "$F/empty"
ln -sf file.txt "$F/rel-link"        # relative target
ln -sf / "$F/root-link"              # absolute target to a stable one-node path
ln -sf rel-link "$F/chain-link"      # symlink -> symlink
ln -sf does-not-exist "$F/dangling"
ln -sf sub "$F/dir-link"             # symlink to a directory
ln -sf ../does-not-exist "$F/nested/deep-link"

# ── help / version ──────────────────────────────────────────────────────────
echo ""
echo "--- help / version ---"
assert_cmd_pat 'namei \[options\] <pathname>' namei --help
assert_cmd_pat 'show mount point directories' namei --help
assert_cmd_pat 'display this help' namei --help
assert_cmd_pat 'vertical align' namei --help
assert_cmd_pat 'namei \(modbox\) 1\.0' namei --version

# ── default (short) format ──────────────────────────────────────────────────
echo ""
echo "--- default format ---"
assert_cmd "f: $F/file.txt
 d $F
 - file.txt" namei "$F/file.txt"

assert_cmd "f: $F/sub/deep
 d $F
 d sub
 d deep" namei "$F/sub/deep"

# Trailing slash drops the last component; the header echoes the input.
assert_cmd "f: $F/sub/
 d $F
 d sub" namei "$F/sub/"

# Double slash: the empty segment is skipped, the header echoes the input.
assert_cmd "f: $F//file.txt
 d $F
 - file.txt" namei "$F//file.txt"

# Empty segments are skipped.
assert_cmd "f: $F///file.txt
 d $F
 - file.txt" namei "$F///file.txt"

# Component types: regular file and empty file.
assert_cmd "f: $F/empty
 d $F
 - empty" namei "$F/empty"

assert_cmd "f: $F/deep
 d $F
 d deep" namei "$F/deep"

# ── multiple paths ──────────────────────────────────────────────────────────
echo ""
echo "--- multiple paths ---"
assert_cmd "f: $F/sub
 d $F
 d sub
f: $F/sub/deep/inner
 d $F
 d sub
 d deep
 d inner" namei "$F/sub" "$F/sub/deep/inner"

# ── modes (-m) ──────────────────────────────────────────────────────────────
echo ""
echo "--- modes (-m) ---"
assert_cmd "f: $F/file.txt
 drwxr-xr-x $F
 -rw-r--r-- file.txt" namei -m "$F/file.txt"

assert_cmd "f: $F/sub
 drwxr-xr-x $F
 drwxr-xr-x sub" namei -m "$F/sub"

assert_cmd "f: $F/rel-link
 drwxr-xr-x $F
 lrwxrwxrwx rel-link -> file.txt
   -rw-r--r-- file.txt" namei -m "$F/rel-link"

# Combined short options (-m -v): the indent moves after the mode.
assert_cmd "f: $F/file.txt
drwxr-xr-x $F
-rw-r--r-- file.txt" namei -m -v "$F/file.txt"

# ── owners (-o) and long (-l) ───────────────────────────────────────────────
echo ""
echo "--- owners / long ---"
assert_cmd_norm "f: $F/file.txt
drwxr-xr-x N N $F
-rw-r--r-- N N file.txt" namei -l "$F/file.txt"

assert_cmd_norm "f: $F/sub/deep
 d N N $F
 d N N sub
 d N N deep" namei -o "$F/sub/deep"

# -o alone still shows the short type char.
assert_cmd_norm "f: $F/sub
 d N N $F
 d N N sub" namei -o "$F/sub"

# Padding across a run: both paths share the widest name column.
assert_cmd_norm "f: $F/sub
 d N N $F
 d N N sub
f: $F/file.txt
 d N N $F
 - N N file.txt" namei -o "$F/sub" "$F/file.txt"

# Symlink owner/group column and the arrow target.
assert_cmd_norm "f: $F/rel-link
 drwxr-xr-x N N $F
 lrwxrwxrwx N N rel-link -> file.txt
   -rw-r--r-- N N file.txt" namei -m -o "$F/rel-link"

# ── vertical alignment (-v) ─────────────────────────────────────────────────
echo ""
echo "--- vertical (-v) ---"
assert_cmd "f: $F/sub/deep
drwxr-xr-x $F
drwxr-xr-x sub
drwxr-xr-x deep" namei -v -m "$F/sub/deep"

# -l == -m -o -v: long modes with the indent after them.
assert_cmd_norm "f: $F/sub/deep
drwxr-xr-x N N $F
drwxr-xr-x N N sub
drwxr-xr-x N N deep" namei -l "$F/sub/deep"

# ── symlinks ────────────────────────────────────────────────────────────────
echo ""
echo "--- symlinks ---"
assert_cmd "f: $F/rel-link
 d $F
 l rel-link -> file.txt
   - file.txt" namei "$F/rel-link"

assert_cmd "f: $F/root-link
 d $F
 l root-link -> /
   d /" namei "$F/root-link"

# Nested symlink: chain-link -> rel-link -> file.txt.
assert_cmd "f: $F/chain-link
 d $F
 l chain-link -> rel-link
   l rel-link -> file.txt
     - file.txt" namei "$F/chain-link"

# Symlink to a directory resolves into its components.
assert_cmd "f: $F/dir-link
 d $F
 l dir-link -> sub
   d sub" namei "$F/dir-link"

# -n shows the link without resolving it.
assert_cmd "f: $F/rel-link
 d $F
 l rel-link -> file.txt" namei -n "$F/rel-link"

assert_cmd "f: $F/chain-link
 d $F
 l chain-link -> rel-link" namei -n "$F/chain-link"

# -n combined with -m keeps the long mode string.
assert_cmd "f: $F/rel-link
 drwxr-xr-x $F
 lrwxrwxrwx rel-link -> file.txt" namei -n -m "$F/rel-link"

# A dangling link reports the missing target component.
assert_cmd "f: $F/dangling
 d $F
 l dangling -> does-not-exist
      does-not-exist - No such file or directory" namei "$F/dangling"
rc=0; "$MODBOX" namei "$F/dangling" >/dev/null 2>&1 || rc=$?
[[ "$rc" -ne 0 ]] && pass "dangling link exits non-zero (rc=$rc)" \
                  || fail "dangling link should exit non-zero"

# Dangling target resolved relative to a deeper directory.
assert_cmd "f: $F/nested/deep-link
 d $F
 d nested
 l deep-link -> ../does-not-exist
   d ..
      does-not-exist - No such file or directory" namei "$F/nested/deep-link"

# ── failures ────────────────────────────────────────────────────────────────
echo ""
echo "--- failures ---"
assert_cmd "f: $F/missing
 d $F
    missing - No such file or directory" namei "$F/missing"

assert_cmd "f: $F/sub/nope/file
 d $F
 d sub
    nope - No such file or directory" namei "$F/sub/nope/file"

assert_cmd "f: nope-at-all
    nope-at-all - No such file or directory" namei nope-at-all

# A failure in a later path still prints the earlier one.
assert_cmd "f: $F/sub
 d $F
 d sub
f: $F/missing
 d $F
    missing - No such file or directory" namei "$F/sub" "$F/missing"

# Failure indentation differs per mode combination.
assert_cmd "f: $F/missing
d $F
   missing - No such file or directory" namei -v "$F/missing"

assert_cmd "f: $F/missing
 drwxr-xr-x $F
             missing - No such file or directory" namei -m "$F/missing"

# No path at all is a usage error.
assert_cmd_pat_stderr 'namei.*missing operand' namei
"$MODBOX" namei >/dev/null 2>&1; rc=$?
[[ "$rc" -eq 2 ]] && pass "no path exits 2" || fail "no path should exit 2, got $rc"

# Unknown option is a usage error (argtable3 wording, non-zero exit).
assert_cmd_pat_stderr "unrecognized option '--nope'" namei --nope "$F/sub"
"$MODBOX" namei --nope "$F/sub" >/dev/null 2>&1; rc=$?
[[ "$rc" -ne 0 ]] && pass "unknown option exits non-zero (rc=$rc)" \
                   || fail "unknown option should exit non-zero"

# Success exits 0.
"$MODBOX" namei "$F/sub/deep" >/dev/null 2>&1; rc=$?
[[ "$rc" -eq 0 ]] && pass "success exits 0" || fail "success should exit 0, got $rc"

# A failed path exits non-zero even when other paths succeeded.
"$MODBOX" namei "$F/sub" "$F/missing" >/dev/null 2>&1; rc=$?
[[ "$rc" -ne 0 ]] && pass "mixed paths exit non-zero (rc=$rc)" \
                  || fail "mixed paths should exit non-zero"

# An empty path argument is skipped but fails the run.
"$MODBOX" namei "" >/dev/null 2>&1; rc=$?
[[ "$rc" -ne 0 ]] && pass "empty path argument exits non-zero (rc=$rc)" \
                  || fail "empty path argument should fail the run"

# --context is not implemented and is rejected as an unrecognized option.
assert_cmd_pat_stderr "unrecognized option '--context'" namei --context "$F/sub"

# ── mount points (-x) ───────────────────────────────────────────────────────
echo ""
echo "--- mount points (-x) ---"
assert_cmd "f: /
 D /" namei -x /

# A relative path has no leading root component, so no D appears here.
assert_cmd "f: $F/sub
 d $F
 d sub" namei -x "$F/sub"

assert_cmd "f: $F
 d $F" namei -x "$F"

# -x combined with -m: the D replaces the type char inside the mode string.
assert_cmd "f: $F
 drwxr-xr-x $F" namei -m -x "$F"

# -x combined with -l.
assert_cmd_norm "f: $F/sub
drwxr-xr-x N N $F
drwxr-xr-x N N sub" namei -l -x "$F/sub"

# ── summary ─────────────────────────────────────────────────────────────────
echo ""
echo "=========================================="
echo "  namei: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "=========================================="
[[ "$FAIL_COUNT" -eq 0 ]] || exit 1
