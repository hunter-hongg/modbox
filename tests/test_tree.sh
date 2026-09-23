#!/usr/bin/env bash
#
# test_tree.sh — modbox tree: directory listing in tree form
#
# Exercises the public CLI seam of `modbox tree`. The fixture is built inside a
# private directory the test cds into, so every listing is written with a
# relative path and the expected output can be compared byte-exactly. Anything
# that depends on the host filesystem — inode numbers, byte sizes, timestamps —
# is checked with a pattern instead of a literal.
#
set -o nounset
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# ── fixture ─────────────────────────────────────────────────────────────────
cd "$TMPDIR"
T=tr
mkdir -p "$T/a/b/deep" "$T/a/c1" "$T/e" "$T/empty" "$T/k"
printf 'one\n'   > "$T/a/f1.txt"
printf 'two\n'   > "$T/a/b/f2.log"
printf 'three\n' > "$T/a/b/deep/f3"
printf 'x\n'     > "$T/a/c1/x.dat"
: > "$T/a/.hid"
ln -s f1.txt "$T/a/link"
ln -s ../a    "$T/e/dirlink"
ln -s nowhere "$T/e/dangling"
: > "$T/k/v1"
: > "$T/k/v2"
: > "$T/k/v10"
printf '#!/bin/sh\n' > "$T/script.sh"
chmod +x "$T/script.sh"
: > "$T/.hidden_top"

cd "$T"

# ── plain listing ───────────────────────────────────────────────────────────
assert_cmd "a
├── b
│   ├── deep
│   │   └── f3
│   └── f2.log
├── c1
│   └── x.dat
├── f1.txt
└── link -> f1.txt

4 directories, 5 files" tree a

# No path at all lists the working directory, named ".".
assert_cmd ".
├── a
│   ├── b
│   │   ├── deep
│   │   │   └── f3
│   │   └── f2.log
│   ├── c1
│   │   └── x.dat
│   ├── f1.txt
│   └── link -> f1.txt
├── e
│   ├── dangling -> nowhere
│   └── dirlink -> ../a
├── empty
├── k
│   ├── v1
│   ├── v10
│   └── v2
└── script.sh

9 directories, 10 files" tree

# A walk root is counted only when it is listed with at least one child, so the
# report is singular for one child directory and empty for a bare one.
assert_cmd "a/b/deep
└── f3

1 directory, 1 file" tree a/b/deep

assert_cmd "empty

0 directories, 0 files" tree empty

assert_cmd "empty" tree --noreport empty

# Hidden entries appear only with -a, and never for "." itself.
assert_cmd "a
├── b
│   ├── deep
│   │   └── f3
│   └── f2.log
├── c1
│   └── x.dat
├── f1.txt
├── .hid
└── link -> f1.txt

4 directories, 6 files" tree -a a

assert_cmd ".
├── a
│   ├── b
│   │   ├── deep
│   │   │   └── f3
│   │   └── f2.log
│   ├── c1
│   │   └── x.dat
│   ├── f1.txt
│   ├── .hid
│   └── link -> f1.txt
├── e
│   ├── dangling -> nowhere
│   └── dirlink -> ../a
├── empty
├── .hidden_top
├── k
│   ├── v1
│   ├── v10
│   └── v2
└── script.sh

9 directories, 12 files" tree -a .

# ── -d: directories only ────────────────────────────────────────────────────
# A symlink to a directory counts as a directory, and -d alone does not follow
# it, so e/ keeps just the link.
assert_cmd ".
├── a
│   ├── b
│   │   └── deep
│   └── c1
├── e
│   └── dirlink -> ../a
├── empty
└── k

9 directories" tree -d .

# ── ordering ────────────────────────────────────────────────────────────────
assert_cmd ".
├── a
│   ├── b
│   │   ├── deep
│   │   │   └── f3
│   │   └── f2.log
│   ├── c1
│   │   └── x.dat
│   ├── f1.txt
│   └── link -> f1.txt
├── e
│   ├── dirlink -> ../a
│   └── dangling -> nowhere
├── empty
├── k
│   ├── v1
│   ├── v10
│   └── v2
└── script.sh

9 directories, 10 files" tree --dirsfirst .

assert_cmd ".
├── script.sh
├── a
│   ├── f1.txt
│   ├── link -> f1.txt
│   ├── b
│   │   ├── f2.log
│   │   └── deep
│   │       └── f3
│   └── c1
│       └── x.dat
├── e
│   ├── dangling -> nowhere
│   └── dirlink -> ../a
├── empty
└── k
    ├── v1
    ├── v10
    └── v2

9 directories, 10 files" tree --filesfirst .

# -r reverses the comparison but keeps the directories-before-files grouping.
assert_cmd ".
├── k
│   ├── v2
│   ├── v10
│   └── v1
├── empty
├── e
│   ├── dirlink -> ../a
│   └── dangling -> nowhere
├── a
│   ├── c1
│   │   └── x.dat
│   ├── b
│   │   ├── deep
│   │   │   └── f3
│   │   └── f2.log
│   ├── link -> f1.txt
│   └── f1.txt
└── script.sh

9 directories, 10 files" tree -r --dirsfirst .

# Version sort reads the digit runs as numbers; -r flips that order too.
assert_cmd "k
├── v1
├── v2
└── v10

1 directory, 3 files" tree -v k

assert_cmd "k
├── v2
├── v10
└── v1

1 directory, 3 files" tree -r k

# ── output shape ────────────────────────────────────────────────────────────
# -i drops the indentation, -f replaces each name with its path from the root.
assert_cmd "a
b
deep
f3
f2.log
c1
x.dat
f1.txt
link -> f1.txt

4 directories, 5 files" tree -i a

assert_cmd "a
├── a/b
│   ├── a/b/deep
│   │   └── a/b/deep/f3
│   └── a/b/f2.log
├── a/c1
│   └── a/c1/x.dat
├── a/f1.txt
└── a/link -> f1.txt

4 directories, 5 files" tree -f a

assert_cmd "a
|-- b
|   |-- deep
|   |   \`-- f3
|   \`-- f2.log
|-- c1
|   \`-- x.dat
|-- f1.txt
\`-- link -> f1.txt

4 directories, 5 files" tree --charset=ascii a

# -F marks directories, the executable and a symlink that resolves to a
# directory; the indicator qualifies the target, so it lands after "-> ../a".
assert_cmd "./
├── a/
│   ├── b/
│   │   ├── deep/
│   │   │   └── f3
│   │   └── f2.log
│   ├── c1/
│   │   └── x.dat
│   ├── f1.txt
│   └── link -> f1.txt
├── e/
│   ├── dangling -> nowhere
│   └── dirlink -> ../a/
├── empty/
├── k/
│   ├── v1
│   ├── v10
│   └── v2
└── script.sh*

9 directories, 10 files" tree -F .

# ── depth limit ─────────────────────────────────────────────────────────────
assert_cmd ".
├── a
├── e
├── empty
├── k
└── script.sh

5 directories, 1 file" tree -L 1 .

assert_cmd "a
├── b
│   ├── deep
│   └── f2.log
├── c1
│   └── x.dat
├── f1.txt
└── link -> f1.txt

4 directories, 4 files" tree -L 2 a

assert_cmd_pat_stderr 'Invalid level' tree -L 0 .
"$MODBOX" tree -L 0 . >/dev/null 2>&1; rc=$?
[[ "$rc" -eq 1 ]] && pass "-L 0 exits 1" || fail "-L 0 should exit 1, got $rc"

# ── pattern filters ─────────────────────────────────────────────────────────
# -I drops any entry whose name matches; -P keeps directories, and files only
# when they match.
assert_cmd "a
├── b
│   └── deep
├── c1
│   └── x.dat
└── link -> f1.txt

4 directories, 2 files" tree -I 'f*' a

assert_cmd "a
├── b
│   └── deep
├── c1
└── f1.txt

4 directories, 1 file" tree -P '*.txt' a

# Patterns are shell-style globs joined by "|", matched against the whole name.
assert_cmd "a
├── b
│   └── deep
└── c1
    └── x.dat

4 directories, 1 file" tree -I 'f*|link' a

assert_cmd "a
├── b
│   └── deep
├── c1
│   └── x.dat
└── f1.txt

4 directories, 2 files" tree -P '*.txt|x.dat' a

# An empty pattern list excludes nothing.
assert_cmd "a
├── b
│   ├── deep
│   │   └── f3
│   └── f2.log
├── c1
│   └── x.dat
├── f1.txt
└── link -> f1.txt

4 directories, 5 files" tree -I '' a

# ── symbolic links ──────────────────────────────────────────────────────────
assert_cmd "e
├── dangling -> nowhere
└── dirlink -> ../a

2 directories, 1 file" tree e

assert_cmd "e
├── dangling -> nowhere
└── dirlink -> ../a
    ├── b
    │   ├── deep
    │   │   └── f3
    │   └── f2.log
    ├── c1
    │   └── x.dat
    ├── f1.txt
    └── link -> f1.txt

5 directories, 6 files" tree -l e

# A link back into a directory already listed stops with a note instead of
# looping; each directory is expanded once.
FAN=fan
mkdir -p "$FAN/x/sub/deep"
: > "$FAN/x/sub/deep/f"
ln -s sub "$FAN/x/s1"
ln -s sub "$FAN/x/s2"
assert_cmd "fan
└── x
    ├── s1 -> sub
    │   └── deep
    │       └── f
    ├── s2 -> sub  [recursive, not followed]
    └── sub
        └── deep
            └── f

7 directories, 2 files" tree -l "$FAN"

# ── multiple roots ──────────────────────────────────────────────────────────
assert_cmd "a
├── b
│   ├── deep
│   │   └── f3
│   └── f2.log
├── c1
│   └── x.dat
├── f1.txt
└── link -> f1.txt
e
├── dangling -> nowhere
└── dirlink -> ../a

6 directories, 6 files" tree a e

# A trailing slash on an argument is not doubled by -f.
assert_cmd "a
├── a/b
│   ├── a/b/deep
│   │   └── a/b/deep/f3
│   └── a/b/f2.log
├── a/c1
│   └── a/c1/x.dat
├── a/f1.txt
└── a/link -> f1.txt

4 directories, 5 files" tree -f a/

# ── metadata columns ────────────────────────────────────────────────────────
# Widths are fixed but the values come from the host filesystem.
assert_cmd_pat '^\[ *[0-9]+\]  a$' tree -s a
assert_cmd_pat '^\[[ 0-9KMGTP.]+\]  a$' tree -h a
assert_cmd_pat '^\[[ 0-9KMGTP.]+\]  a$' tree --du -h a
assert_cmd_pat '^\[[0-9 ]{7,}\]  a$' tree --inodes a
assert_cmd_pat '^\[[A-Z][a-z]{2} [ 0-9]{2} ([0-9]{2}:[0-9]{2}| [0-9]{4})\]  a$' tree -D a

# -s and --du share one bracket with the inode column.
assert_cmd_pat '^\[[0-9 ]{7,}\]  a$' tree --inodes -s a

# ── unreadable and missing paths ────────────────────────────────────────────
# Created late: an unreadable directory would otherwise show up in every
# top-level listing above.
mkdir -p lock/sub
: > lock/sub/secret
chmod 000 lock

assert_cmd "lock  [error opening dir]

0 directories, 1 file" tree lock

assert_cmd "nope  [error opening dir]

0 directories, 0 files" tree nope
"$MODBOX" tree nope >/dev/null 2>&1; rc=$?
[[ "$rc" -eq 2 ]] && pass "missing path exits 2" || fail "missing path should exit 2, got $rc"

# One bad path fails the run even when another path listed cleanly.
"$MODBOX" tree a nope >/dev/null 2>&1; rc=$?
[[ "$rc" -eq 2 ]] && pass "mixed paths exit 2 (rc=$rc)" || fail "mixed paths should exit 2, got $rc"

chmod 755 lock

# ── usage and version ───────────────────────────────────────────────────────
assert_cmd_pat 'modbox' tree --version
assert_cmd_pat 'Usage: tree' tree --help
assert_cmd_pat_stderr 'unrecognized option' tree --nope a
"$MODBOX" tree --nope a >/dev/null 2>&1; rc=$?
[[ "$rc" -ne 0 ]] && pass "unknown option exits non-zero (rc=$rc)" \
                  || fail "unknown option should exit non-zero"

# GNU tree has no -Z short form; only the long --selinux is accepted, and
# argtable3 reports the rejected option letter.
assert_cmd_pat_stderr 'unexpected argument' tree -Z a
"$MODBOX" tree -Z a >/dev/null 2>&1; rc=$?
[[ "$rc" -ne 0 ]] && pass "-Z exits non-zero (rc=$rc)" || fail "-Z should exit non-zero"

# ── summary ─────────────────────────────────────────────────────────────────
echo ""
echo "=========================================="
echo "  tree: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "=========================================="
[[ "$FAIL_COUNT" -eq 0 ]] || exit 1
