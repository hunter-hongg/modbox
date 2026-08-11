#!/usr/bin/env bash
#
# test_man_pages.sh — Test man page generation and installation
#

# shellcheck source=framework.sh
source "$(dirname "${BASH_SOURCE[0]}")/framework.sh"

echo "=== Man Page Tests ==="

# Test that all three man page sources exist
if [[ -f "docs/man/modbox-cat.1.md" ]]; then
    pass "docs/man/modbox-cat.1.md exists"
else
    fail "docs/man/modbox-cat.1.md missing"
fi

if [[ -f "docs/man/modbox-ls.1.md" ]]; then
    pass "docs/man/modbox-ls.1.md exists"
else
    fail "docs/man/modbox-ls.1.md missing"
fi

if [[ -f "docs/man/modbox-rm.1.md" ]]; then
    pass "docs/man/modbox-rm.1.md exists"
else
    fail "docs/man/modbox-rm.1.md missing"
fi

if [[ -f "docs/man/modbox-cp.1.md" ]]; then
    pass "docs/man/modbox-cp.1.md exists"
else
    fail "docs/man/modbox-cp.1.md missing"
fi

if [[ -f "docs/man/modbox-mv.1.md" ]]; then
    pass "docs/man/modbox-mv.1.md exists"
else
    fail "docs/man/modbox-mv.1.md missing"
fi

if [[ -f "docs/man/modbox-rmdir.1.md" ]]; then
    pass "docs/man/modbox-rmdir.1.md exists"
else
    fail "docs/man/modbox-rmdir.1.md missing"
fi

if [[ -f "docs/man/modbox-arch.1.md" ]]; then
    pass "docs/man/modbox-arch.1.md exists"
else
    fail "docs/man/modbox-arch.1.md missing"
fi

if [[ -f "docs/man/modbox-audit2allow.1.md" ]]; then
    pass "docs/man/modbox-audit2allow.1.md exists"
else
    fail "docs/man/modbox-audit2allow.1.md missing"
fi

if [[ -f "docs/man/modbox-awk.1.md" ]]; then
    pass "docs/man/modbox-awk.1.md exists"
else
    fail "docs/man/modbox-awk.1.md missing"
fi

if [[ -f "docs/man/modbox-head.1.md" ]]; then
    pass "docs/man/modbox-head.1.md exists"
else
    fail "docs/man/modbox-head.1.md missing"
fi

if [[ -f "docs/man/modbox-tail.1.md" ]]; then
    pass "docs/man/modbox-tail.1.md exists"
else
    fail "docs/man/modbox-tail.1.md missing"
fi

if [[ -f "docs/man/modbox-sort.1.md" ]]; then
    pass "docs/man/modbox-sort.1.md exists"
else
    fail "docs/man/modbox-sort.1.md missing"
fi

if [[ -f "docs/man/modbox-grep.1.md" ]]; then
    pass "docs/man/modbox-grep.1.md exists"
else
    fail "docs/man/modbox-grep.1.md missing"
fi

if [[ -f "docs/man/modbox-sed.1.md" ]]; then
    pass "docs/man/modbox-sed.1.md exists"
else
    fail "docs/man/modbox-sed.1.md missing"
fi

if [[ -f "docs/man/modbox-find.1.md" ]]; then
    pass "docs/man/modbox-find.1.md exists"
else
    fail "docs/man/modbox-find.1.md missing"
fi

if [[ -f "docs/man/modbox-stat.1.md" ]]; then
    pass "docs/man/modbox-stat.1.md exists"
else
    fail "docs/man/modbox-stat.1.md missing"
fi

if [[ -f "docs/man/modbox-diff.1.md" ]]; then
    pass "docs/man/modbox-diff.1.md exists"
else
    fail "docs/man/modbox-diff.1.md missing"
fi

if [[ -f "docs/man/modbox-cut.1.md" ]]; then
    pass "docs/man/modbox-cut.1.md exists"
else
    fail "docs/man/modbox-cut.1.md missing"
fi

if [[ -f "docs/man/modbox-date.1.md" ]]; then
    pass "docs/man/modbox-date.1.md exists"
else
    fail "docs/man/modbox-date.1.md missing"
fi

if [[ -f "docs/man/modbox-uniq.1.md" ]]; then
    pass "docs/man/modbox-uniq.1.md exists"
else
    fail "docs/man/modbox-uniq.1.md missing"
fi

if [[ -f "docs/man/modbox-tee.1.md" ]]; then
    pass "docs/man/modbox-tee.1.md exists"
else
    fail "docs/man/modbox-tee.1.md missing"
fi

if [[ -f "docs/man/modbox-wc.1.md" ]]; then
    pass "docs/man/modbox-wc.1.md exists"
else
    fail "docs/man/modbox-wc.1.md missing"
fi

if [[ -f "docs/man/modbox-mkdir.1.md" ]]; then
    pass "docs/man/modbox-mkdir.1.md exists"
else
    fail "docs/man/modbox-mkdir.1.md missing"
fi

if [[ -f "docs/man/modbox-chmod.1.md" ]]; then
    pass "docs/man/modbox-chmod.1.md exists"
else
    fail "docs/man/modbox-chmod.1.md missing"
fi

if [[ -f "docs/man/modbox-chown.1.md" ]]; then
    pass "docs/man/modbox-chown.1.md exists"
else
    fail "docs/man/modbox-chown.1.md missing"
fi

if [[ -f "docs/man/modbox-ln.1.md" ]]; then
    pass "docs/man/modbox-ln.1.md exists"
else
    fail "docs/man/modbox-ln.1.md missing"
fi

if [[ -f "docs/man/modbox-dd.1.md" ]]; then
    pass "docs/man/modbox-dd.1.md exists"
else
    fail "docs/man/modbox-dd.1.md missing"
fi

if [[ -f "docs/man/modbox-tr.1.md" ]]; then
    pass "docs/man/modbox-tr.1.md exists"
else
    fail "docs/man/modbox-tr.1.md missing"
fi

if [[ -f "docs/man/modbox-uname.1.md" ]]; then
    pass "docs/man/modbox-uname.1.md exists"
else
    fail "docs/man/modbox-uname.1.md missing"
fi

if [[ -f "docs/man/modbox-echo.1.md" ]]; then
    pass "docs/man/modbox-echo.1.md exists"
else
    fail "docs/man/modbox-echo.1.md missing"
fi

if [[ -f "docs/man/modbox-xargs.1.md" ]]; then
    pass "docs/man/modbox-xargs.1.md exists"
else
    fail "docs/man/modbox-xargs.1.md missing"
fi

if [[ -f "docs/man/modbox-basename.1.md" ]]; then
    pass "docs/man/modbox-basename.1.md exists"
else
    fail "docs/man/modbox-basename.1.md missing"
fi

if [[ -f "docs/man/modbox-dirname.1.md" ]]; then
    pass "docs/man/modbox-dirname.1.md exists"
else
    fail "docs/man/modbox-dirname.1.md missing"
fi

if [[ -f "docs/man/modbox-pwd.1.md" ]]; then
    pass "docs/man/modbox-pwd.1.md exists"
else
    fail "docs/man/modbox-pwd.1.md missing"
fi

if [[ -f "docs/man/modbox-sleep.1.md" ]]; then
    pass "docs/man/modbox-sleep.1.md exists"
else
    fail "docs/man/modbox-sleep.1.md missing"
fi

if [[ -f "docs/man/modbox-touch.1.md" ]]; then
    pass "docs/man/modbox-touch.1.md exists"
else
    fail "docs/man/modbox-touch.1.md missing"
fi
if [[ -f "docs/man/modbox-mktemp.1.md" ]]; then
    pass "docs/man/modbox-mktemp.1.md exists"
else
    fail "docs/man/modbox-mktemp.1.md missing"
fi

if [[ -f "docs/man/modbox-install.1.md" ]]; then
    pass "docs/man/modbox-install.1.md exists"
else
    fail "docs/man/modbox-install.1.md missing"
fi

if [[ -f "docs/man/modbox-seq.1.md" ]]; then
    pass "docs/man/modbox-seq.1.md exists"
else
    fail "docs/man/modbox-seq.1.md missing"
fi

if [[ -f "docs/man/modbox-comm.1.md" ]]; then
    pass "docs/man/modbox-comm.1.md exists"
else
    fail "docs/man/modbox-comm.1.md missing"
fi

if [[ -f "docs/man/modbox-paste.1.md" ]]; then
    pass "docs/man/modbox-paste.1.md exists"
else
    fail "docs/man/modbox-paste.1.md missing"
fi

if [[ -f "docs/man/modbox-nl.1.md" ]]; then
    pass "docs/man/modbox-nl.1.md exists"
else
    fail "docs/man/modbox-nl.1.md missing"
fi

# Test that Makefile has required targets and variables

if grep -q "^man:" Makefile; then
    pass "Makefile has man target"
else
    fail "Makefile missing man target"
fi

if grep -q "^install-man:" Makefile; then
    pass "Makefile has install-man target"
else
    fail "Makefile missing install-man target"
fi

if grep -q "^uninstall-man:" Makefile; then
    pass "Makefile has uninstall-man target"
else
    fail "Makefile missing uninstall-man target"
fi

if grep -q "pandoc not found" Makefile; then
    pass "Makefile has pandoc check"
else
    fail "Makefile missing pandoc check"
fi

if grep -q "modbox-cat.1.md" Makefile && grep -q "modbox-ls.1.md" Makefile && grep -q "modbox-rm.1.md" Makefile && grep -q "modbox-cp.1.md" Makefile && grep -q "modbox-mv.1.md" Makefile && grep -q "modbox-rmdir.1.md" Makefile && grep -q "modbox-arch.1.md" Makefile && grep -q "modbox-audit2allow.1.md" Makefile && grep -q "modbox-awk.1.md" Makefile && grep -q "modbox-head.1.md" Makefile && grep -q "modbox-tail.1.md" Makefile && grep -q "modbox-sort.1.md" Makefile && grep -q "modbox-grep.1.md" Makefile && grep -q "modbox-sed.1.md" Makefile && grep -q "modbox-find.1.md" Makefile && grep -q "modbox-stat.1.md" Makefile && grep -q "modbox-diff.1.md" Makefile && grep -q "modbox-cut.1.md" Makefile && grep -q "modbox-date.1.md" Makefile && grep -q "modbox-uniq.1.md" Makefile && grep -q "modbox-tee.1.md" Makefile && grep -q "modbox-wc.1.md" Makefile && grep -q "modbox-mkdir.1.md" Makefile && grep -q "modbox-chmod.1.md" Makefile && grep -q "modbox-chown.1.md" Makefile && grep -q "modbox-ln.1.md" Makefile && grep -q "modbox-dd.1.md" Makefile && grep -q "modbox-tr.1.md" Makefile && grep -q "modbox-uname.1.md" Makefile && grep -q "modbox-echo.1.md" Makefile && grep -q "modbox-xargs.1.md" Makefile; then
    pass "Makefile lists all man page sources"
else
    fail "Makefile missing man page sources"
fi

# Test that Makefile lists the new checksum/encoding man page sources
if grep -q "modbox-md5sum.1.md" Makefile && grep -q "modbox-sha512sum.1.md" Makefile && grep -q "modbox-basenc.1.md" Makefile && grep -q "modbox-sum.1.md" Makefile; then
    pass "Makefile lists checksum/encoding man page sources"
else
    fail "Makefile missing checksum/encoding man page sources"
fi

# Test that Makefile lists the new basename/dirname/pwd/sleep man page sources
if grep -q "modbox-basename.1.md" Makefile && grep -q "modbox-dirname.1.md" Makefile && grep -q "modbox-pwd.1.md" Makefile && grep -q "modbox-sleep.1.md" Makefile; then
    pass "Makefile lists basename/dirname/pwd/sleep man page sources"
else
    fail "Makefile missing basename/dirname/pwd/sleep man page sources"
fi

# Test that Makefile lists the new touch/readlink/mktemp/install/seq/comm/paste/nl man page sources
if grep -q "modbox-touch.1.md" Makefile && grep -q "modbox-readlink.1.md" Makefile && grep -q "modbox-mktemp.1.md" Makefile && grep -q "modbox-install.1.md" Makefile && grep -q "modbox-seq.1.md" Makefile && grep -q "modbox-comm.1.md" Makefile && grep -q "modbox-paste.1.md" Makefile && grep -q "modbox-nl.1.md" Makefile; then
    pass "Makefile lists touch/readlink/mktemp/install/seq/comm/paste/nl man page sources"
else
    fail "Makefile missing touch/readlink/mktemp/install/seq/comm/paste/nl man page sources"
fi



if grep -q "gzip -9" Makefile; then

    pass "Makefile uses gzip compression for install"
else
    fail "Makefile missing gzip compression"
fi

echo ""
echo "=== Man Page Content Tests (requires pandoc) ==="

# Only run pandoc-dependent tests if pandoc is available
if command -v pandoc >/dev/null 2>&1; then
    # Generate man pages
    make man >/dev/null 2>&1 || true

    # Test that generated files exist
    if [[ -f "build/man/modbox-cat.1" ]]; then
        pass "build/man/modbox-cat.1 generated"
    else
        fail "build/man/modbox-cat.1 not generated"
    fi

    if [[ -f "build/man/modbox-ls.1" ]]; then
        pass "build/man/modbox-ls.1 generated"
    else
        fail "build/man/modbox-ls.1 not generated"
    fi

    if [[ -f "build/man/modbox-rm.1" ]]; then
        pass "build/man/modbox-rm.1 generated"
    else
        fail "build/man/modbox-rm.1 not generated"
    fi

    if [[ -f "build/man/modbox-cp.1" ]]; then
        pass "build/man/modbox-cp.1 generated"
    else
        fail "build/man/modbox-cp.1 not generated"
    fi

    if [[ -f "build/man/modbox-mv.1" ]]; then
        pass "build/man/modbox-mv.1 generated"
    else
        fail "build/man/modbox-mv.1 not generated"
    fi

    if [[ -f "build/man/modbox-rmdir.1" ]]; then
        pass "build/man/modbox-rmdir.1 generated"
    else
        fail "build/man/modbox-rmdir.1 not generated"
    fi

    if [[ -f "build/man/modbox-arch.1" ]]; then
        pass "build/man/modbox-arch.1 generated"
    else
        fail "build/man/modbox-arch.1 not generated"
    fi

    if [[ -f "build/man/modbox-audit2allow.1" ]]; then
        pass "build/man/modbox-audit2allow.1 generated"
    else
        fail "build/man/modbox-audit2allow.1 not generated"
    fi

    if [[ -f "build/man/modbox-awk.1" ]]; then
        pass "build/man/modbox-awk.1 generated"
    else
        fail "build/man/modbox-awk.1 not generated"
    fi

    if [[ -f "build/man/modbox-head.1" ]]; then
        pass "build/man/modbox-head.1 generated"
    else
        fail "build/man/modbox-head.1 not generated"
    fi

    if [[ -f "build/man/modbox-tail.1" ]]; then
        pass "build/man/modbox-tail.1 generated"
    else
        fail "build/man/modbox-tail.1 not generated"
    fi

    if [[ -f "build/man/modbox-sort.1" ]]; then
        pass "build/man/modbox-sort.1 generated"
    else
        fail "build/man/modbox-sort.1 not generated"
    fi

    if [[ -f "build/man/modbox-grep.1" ]]; then
        pass "build/man/modbox-grep.1 generated"
    else
        fail "build/man/modbox-grep.1 not generated"
    fi

    if [[ -f "build/man/modbox-sed.1" ]]; then
        pass "build/man/modbox-sed.1 generated"
    else
        fail "build/man/modbox-sed.1 not generated"
    fi

    if [[ -f "build/man/modbox-find.1" ]]; then
        pass "build/man/modbox-find.1 generated"
    else
        fail "build/man/modbox-find.1 not generated"
    fi

    if [[ -f "build/man/modbox-stat.1" ]]; then
        pass "build/man/modbox-stat.1 generated"
    else
        fail "build/man/modbox-stat.1 not generated"
    fi

    if [[ -f "build/man/modbox-diff.1" ]]; then
        pass "build/man/modbox-diff.1 generated"
    else
        fail "build/man/modbox-diff.1 not generated"
    fi

    if [[ -f "build/man/modbox-cut.1" ]]; then
        pass "build/man/modbox-cut.1 generated"
    else
        fail "build/man/modbox-cut.1 not generated"
    fi

    if [[ -f "build/man/modbox-date.1" ]]; then
        pass "build/man/modbox-date.1 generated"
    else
        fail "build/man/modbox-date.1 not generated"
    fi

    if [[ -f "build/man/modbox-uniq.1" ]]; then
        pass "build/man/modbox-uniq.1 generated"
    else
        fail "build/man/modbox-uniq.1 not generated"
    fi

    if [[ -f "build/man/modbox-tee.1" ]]; then
        pass "build/man/modbox-tee.1 generated"
    else
        fail "build/man/modbox-tee.1 not generated"
    fi

    if [[ -f "build/man/modbox-wc.1" ]]; then
        pass "build/man/modbox-wc.1 generated"
    else
        fail "build/man/modbox-wc.1 not generated"
    fi

    if [[ -f "build/man/modbox-mkdir.1" ]]; then
        pass "build/man/modbox-mkdir.1 generated"
    else
        fail "build/man/modbox-mkdir.1 not generated"
    fi

    if [[ -f "build/man/modbox-chmod.1" ]]; then
        pass "build/man/modbox-chmod.1 generated"
    else
        fail "build/man/modbox-chmod.1 not generated"
    fi

    if [[ -f "build/man/modbox-chown.1" ]]; then
        pass "build/man/modbox-chown.1 generated"
    else
        fail "build/man/modbox-chown.1 not generated"
    fi

    if [[ -f "build/man/modbox-ln.1" ]]; then
        pass "build/man/modbox-ln.1 generated"
    else
        fail "build/man/modbox-ln.1 not generated"
    fi

    if [[ -f "build/man/modbox-dd.1" ]]; then
        pass "build/man/modbox-dd.1 generated"
    else
        fail "build/man/modbox-dd.1 not generated"
    fi

    if [[ -f "build/man/modbox-tr.1" ]]; then
        pass "build/man/modbox-tr.1 generated"
    else
        fail "build/man/modbox-tr.1 not generated"
    fi

    if [[ -f "build/man/modbox-uname.1" ]]; then
        pass "build/man/modbox-uname.1 generated"
    else
        fail "build/man/modbox-uname.1 not generated"
    fi

    if [[ -f "build/man/modbox-echo.1" ]]; then
        pass "build/man/modbox-echo.1 generated"
    else
        fail "build/man/modbox-echo.1 not generated"
    fi

    if [[ -f "build/man/modbox-xargs.1" ]]; then
        pass "build/man/modbox-xargs.1 generated"
    else
        fail "build/man/modbox-xargs.1 not generated"
    fi

    if [[ -f "build/man/modbox-basename.1" ]]; then
        pass "build/man/modbox-basename.1 generated"
    else
        fail "build/man/modbox-basename.1 not generated"
    fi

    if [[ -f "build/man/modbox-dirname.1" ]]; then
        pass "build/man/modbox-dirname.1 generated"
    else
        fail "build/man/modbox-dirname.1 not generated"
    fi

    if [[ -f "build/man/modbox-pwd.1" ]]; then
        pass "build/man/modbox-pwd.1 generated"
    else
        fail "build/man/modbox-pwd.1 not generated"
    fi

    if [[ -f "build/man/modbox-sleep.1" ]]; then
        pass "build/man/modbox-sleep.1 generated"
    else
        fail "build/man/modbox-sleep.1 not generated"
    fi

    if [[ -f "build/man/modbox-touch.1" ]]; then
        pass "build/man/modbox-touch.1 generated"
    else
        fail "build/man/modbox-touch.1 not generated"
    fi

    if [[ -f "build/man/modbox-readlink.1" ]]; then
        pass "build/man/modbox-readlink.1 generated"
    else
        fail "build/man/modbox-readlink.1 not generated"
    fi

    if [[ -f "build/man/modbox-mktemp.1" ]]; then
        pass "build/man/modbox-mktemp.1 generated"
    else
        fail "build/man/modbox-mktemp.1 not generated"
    fi

    if [[ -f "build/man/modbox-install.1" ]]; then
        pass "build/man/modbox-install.1 generated"
    else
        fail "build/man/modbox-install.1 not generated"
    fi

    if [[ -f "build/man/modbox-seq.1" ]]; then
        pass "build/man/modbox-seq.1 generated"
    else
        fail "build/man/modbox-seq.1 not generated"
    fi

    if [[ -f "build/man/modbox-comm.1" ]]; then
        pass "build/man/modbox-comm.1 generated"
    else
        fail "build/man/modbox-comm.1 not generated"
    fi

    if [[ -f "build/man/modbox-paste.1" ]]; then
        pass "build/man/modbox-paste.1 generated"
    else
        fail "build/man/modbox-paste.1 not generated"
    fi

    if [[ -f "build/man/modbox-nl.1" ]]; then
        pass "build/man/modbox-nl.1 generated"
    else
        fail "build/man/modbox-nl.1 not generated"
    fi

    # Test that cat man page contains key options

    if man ./build/man/modbox-cat.1 2>/dev/null | col -b | grep -q "number-nonblank"; then
        pass "man page contains -b/--number-nonblank"
    else
        fail "man page missing -b/--number-nonblank"
    fi

    if man ./build/man/modbox-cat.1 2>/dev/null | col -b | grep -q "show-ends"; then
        pass "man page contains -E/--show-ends"
    else
        fail "man page missing -E/--show-ends"
    fi

    if man ./build/man/modbox-cat.1 2>/dev/null | col -b | grep -q "number"; then
        pass "man page contains -n/--number"
    else
        fail "man page missing -n/--number"
    fi

    # Test that ls man page contains key options
    if man ./build/man/modbox-ls.1 2>/dev/null | col -b | grep -q "all"; then
        pass "man page contains -a/--all"
    else
        fail "man page missing -a/--all"
    fi

    if man ./build/man/modbox-ls.1 2>/dev/null | col -b | grep -q "long"; then
        pass "man page contains -l/--long"
    else
        fail "man page missing -l/--long"
    fi

    if man ./build/man/modbox-ls.1 2>/dev/null | col -b | grep -q "reverse"; then
        pass "man page contains -r/--reverse (ls)"
    else
        fail "man page missing -r/--reverse (ls)"
    fi

    # Test that rm man page contains key options
    if man ./build/man/modbox-rm.1 2>/dev/null | col -b | grep -q "recursive"; then
        pass "man page contains -r/--recursive"
    else
        fail "man page missing -r/--recursive"
    fi

    if man ./build/man/modbox-rm.1 2>/dev/null | col -b | grep -q "force"; then
        pass "man page contains -f/--force"
    else
        fail "man page missing -f/--force"
    fi

    if man ./build/man/modbox-rm.1 2>/dev/null | col -b | grep -q "trash"; then
        pass "man page contains --trash"
    else
        fail "man page missing --trash"
    fi

    # Test that cp man page contains key options
    if man ./build/man/modbox-cp.1 2>/dev/null | col -b | grep -q "recursive"; then
        pass "man page contains -r/--recursive (cp)"
    else
        fail "man page missing -r/--recursive (cp)"
    fi

    if man ./build/man/modbox-cp.1 2>/dev/null | col -b | grep -q "preserve"; then
        pass "man page contains -p/--preserve (cp)"
    else
        fail "man page missing -p/--preserve (cp)"
    fi

    if man ./build/man/modbox-cp.1 2>/dev/null | col -b | grep -q "target-directory"; then
        pass "man page contains -t/--target-directory (cp)"
    else
        fail "man page missing -t/--target-directory (cp)"
    fi

    # Test that mv man page contains key options
    if man ./build/man/modbox-mv.1 2>/dev/null | col -b | grep -q "backup"; then
        pass "man page contains -b/--backup (mv)"
    else
        fail "man page missing -b/--backup (mv)"
    fi

    if man ./build/man/modbox-mv.1 2>/dev/null | col -b | grep -q "no-target-directory"; then
        pass "man page contains -T/--no-target-directory (mv)"
    else
        fail "man page missing -T/--no-target-directory (mv)"
    fi

    if man ./build/man/modbox-mv.1 2>/dev/null | col -b | grep -q "update"; then
        pass "man page contains -u/--update (mv)"
    else
        fail "man page missing -u/--update (mv)"
    fi

    # Test that rmdir man page contains key options
    if man ./build/man/modbox-rmdir.1 2>/dev/null | col -b | grep -q "parents"; then
        pass "man page contains -p/--parents (rmdir)"
    else
        fail "man page missing -p/--parents (rmdir)"
    fi

    # Test that arch man page contains key options
    if man ./build/man/modbox-arch.1 2>/dev/null | col -b | grep -q "help"; then
        pass "man page contains --help (arch)"
    else
        fail "man page missing --help (arch)"
    fi

    if man ./build/man/modbox-arch.1 2>/dev/null | col -b | grep -q "version"; then
        pass "man page contains --version (arch)"
    else
        fail "man page missing --version (arch)"
    fi

    # Test that audit2allow man page contains key options
    if man ./build/man/modbox-audit2allow.1 2>/dev/null | col -b | grep -q "input"; then
        pass "man page contains -i/--input (audit2allow)"
    else
        fail "man page missing -i/--input (audit2allow)"
    fi

    if man ./build/man/modbox-audit2allow.1 2>/dev/null | col -b | grep -q "module"; then
        pass "man page contains -m/--module (audit2allow)"
    else
        fail "man page missing -m/--module (audit2allow)"
    fi

    if man ./build/man/modbox-audit2allow.1 2>/dev/null | col -b | grep -q "dontaudit"; then
        pass "man page contains -D/--dontaudit (audit2allow)"
    else
        fail "man page missing -D/--dontaudit (audit2allow)"
    fi

    if man ./build/man/modbox-audit2allow.1 2>/dev/null | col -b | grep -q "requires"; then
        pass "man page contains -r/--requires (audit2allow)"
    else
        fail "man page missing -r/--requires (audit2allow)"
    fi

    if man ./build/man/modbox-audit2allow.1 2>/dev/null | col -b | grep -q "why"; then
        pass "man page contains -w/--why (audit2allow)"
    else
        fail "man page missing -w/--why (audit2allow)"
    fi

    # Test that awk man page contains key content
    if man ./build/man/modbox-awk.1 2>/dev/null | col -b | grep -q "split"; then
        pass "man page contains split function (awk)"
    else
        fail "man page missing split function (awk)"
    fi

    if man ./build/man/modbox-awk.1 2>/dev/null | col -b | grep -q "BEGINFILE"; then
        pass "man page mentions BEGINFILE (awk)"
    else
        fail "man page missing BEGINFILE note (awk)"
    fi

    if man ./build/man/modbox-awk.1 2>/dev/null | col -b | grep -q "for.*in"; then
        pass "man page contains for-in (awk)"
    else
        fail "man page missing for-in (awk)"
    fi

    if man ./build/man/modbox-awk.1 2>/dev/null | col -b | grep -q "OFS"; then
        pass "man page contains OFS variable (awk)"
    else
        fail "man page missing OFS variable (awk)"
    fi

    if man ./build/man/modbox-awk.1 2>/dev/null | col -b | grep -q "gsub"; then
        pass "man page contains gsub function (awk)"
    else
        fail "man page missing gsub function (awk)"
    fi

    # Test that head man page contains key options
    if man ./build/man/modbox-head.1 2>/dev/null | col -b | grep -q "lines"; then
        pass "man page contains --lines (head)"
    else
        fail "man page missing --lines (head)"
    fi

    if man ./build/man/modbox-head.1 2>/dev/null | col -b | grep -q "bytes"; then
        pass "man page contains --bytes (head)"
    else
        fail "man page missing --bytes (head)"
    fi

    # Test that tail man page contains key options
    if man ./build/man/modbox-tail.1 2>/dev/null | col -b | grep -q "follow"; then
        pass "man page contains --follow (tail)"
    else
        fail "man page missing --follow (tail)"
    fi

    if man ./build/man/modbox-tail.1 2>/dev/null | col -b | grep -q "reopen"; then
        pass "man page contains reopen note (tail)"
    else
        fail "man page missing reopen note (tail)"
    fi

    # Test that sort man page contains key options
    if man ./build/man/modbox-sort.1 2>/dev/null | col -b | grep -q "numeric-sort"; then
        pass "man page contains --numeric-sort (sort)"
    else
        fail "man page missing --numeric-sort (sort)"
    fi

    if man ./build/man/modbox-sort.1 2>/dev/null | col -b | grep -q "key"; then
        pass "man page contains --key (sort)"
    else
        fail "man page missing --key (sort)"
    fi

    if man ./build/man/modbox-sort.1 2>/dev/null | col -b | grep -q "field-separator"; then
        pass "man page contains --field-separator (sort)"
    else
        fail "man page missing --field-separator (sort)"
    fi

    if man ./build/man/modbox-sort.1 2>/dev/null | col -b | grep -q "ignore-leading-blanks"; then
        pass "man page contains --ignore-leading-blanks (sort)"
    else
        fail "man page missing --ignore-leading-blanks (sort)"
    fi

    if man ./build/man/modbox-sort.1 2>/dev/null | col -b | grep -q "C locale"; then
        pass "man page contains C locale note (sort)"
    else
        fail "man page missing C locale note (sort)"
    fi

    # Test that grep man page contains key options
    if man ./build/man/modbox-grep.1 2>/dev/null | col -b | grep -q "extended-regexp"; then
        pass "man page contains --extended-regexp (grep)"
    else
        fail "man page missing --extended-regexp (grep)"
    fi

    if man ./build/man/modbox-grep.1 2>/dev/null | col -b | grep -q "invert-match"; then
        pass "man page contains --invert-match (grep)"
    else
        fail "man page missing --invert-match (grep)"
    fi

    if man ./build/man/modbox-grep.1 2>/dev/null | col -b | grep -q "color"; then
        pass "man page contains --color (grep)"
    else
        fail "man page missing --color (grep)"
    fi

    if man ./build/man/modbox-grep.1 2>/dev/null | col -b | grep -q "recursive"; then
        pass "man page contains --recursive (grep)"
    else
        fail "man page missing --recursive (grep)"
    fi

    # Test that sed man page contains key options
    if man ./build/man/modbox-sed.1 2>/dev/null | col -b | grep -q "Substitute"; then
        pass "man page contains substitute command (sed)"
    else
        fail "man page missing substitute command (sed)"
    fi

    if man ./build/man/modbox-sed.1 2>/dev/null | col -b | grep -q "Delete"; then
        pass "man page contains delete command (sed)"
    else
        fail "man page missing delete command (sed)"
    fi

    if man ./build/man/modbox-sed.1 2>/dev/null | col -b | grep -q "in-place"; then
        pass "man page contains --in-place (sed)"
    else
        fail "man page missing --in-place (sed)"
    fi

    # Test that find man page contains key options
    if man ./build/man/modbox-find.1 2>/dev/null | col -b | grep -q "maxdepth"; then
        pass "man page contains --maxdepth (find)"
    else
        fail "man page missing --maxdepth (find)"
    fi

    if man ./build/man/modbox-find.1 2>/dev/null | col -b | grep -q "name PATTERN"; then
        pass "man page contains -name predicate (find)"
    else
        fail "man page missing -name predicate (find)"
    fi

    if man ./build/man/modbox-find.1 2>/dev/null | col -b | grep -q "exec"; then
        pass "man page contains -exec action (find)"
    else
        fail "man page missing -exec action (find)"
    fi

    if man ./build/man/modbox-find.1 2>/dev/null | col -b | grep -q "json"; then
        pass "man page contains --json mode (find)"
    else
        fail "man page missing --json mode (find)"
    fi

    # Test that stat man page contains key options
    if man ./build/man/modbox-stat.1 2>/dev/null | col -b | grep -q "dereference"; then
        pass "man page contains --dereference (stat)"
    else
        fail "man page missing --dereference (stat)"
    fi

    if man ./build/man/modbox-stat.1 2>/dev/null | col -b | grep -q "file-system"; then
        pass "man page contains --file-system (stat)"
    else
        fail "man page missing --file-system (stat)"
    fi

    if man ./build/man/modbox-stat.1 2>/dev/null | col -b | grep -q "%a"; then
        pass "man page contains %a format specifier (stat)"
    else
        fail "man page missing %a format specifier (stat)"
    fi

    if man ./build/man/modbox-stat.1 2>/dev/null | col -b | grep -q "printf"; then
        pass "man page contains --printf (stat)"
    else
        fail "man page missing --printf (stat)"
    fi

    if man ./build/man/modbox-stat.1 2>/dev/null | col -b | grep -q "json"; then
        pass "man page contains --json (stat)"
    else
        fail "man page missing --json (stat)"
    fi

    # Test that diff man page contains key options
    if man ./build/man/modbox-diff.1 2>/dev/null | col -b | grep -q "unified"; then
        pass "man page contains unified format (diff)"
    else
        fail "man page missing unified format (diff)"
    fi

    if man ./build/man/modbox-diff.1 2>/dev/null | col -b | grep -q "context"; then
        pass "man page contains context format (diff)"
    else
        fail "man page missing context format (diff)"
    fi

    if man ./build/man/modbox-diff.1 2>/dev/null | col -b | grep -q "brief"; then
        pass "man page contains --brief (diff)"
    else
        fail "man page missing --brief (diff)"
    fi

    if man ./build/man/modbox-diff.1 2>/dev/null | col -b | grep -q "color"; then
        pass "man page contains --color (diff)"
    else
        fail "man page missing --color (diff)"
    fi

    # Test that cut man page contains key options
    if man ./build/man/modbox-cut.1 2>/dev/null | col -b | grep -q "bytes"; then
        pass "man page contains --bytes (cut)"
    else
        fail "man page missing --bytes (cut)"
    fi

    if man ./build/man/modbox-cut.1 2>/dev/null | col -b | grep -q "characters"; then
        pass "man page contains --characters (cut)"
    else
        fail "man page missing --characters (cut)"
    fi

    if man ./build/man/modbox-cut.1 2>/dev/null | col -b | grep -q "fields"; then
        pass "man page contains --fields (cut)"
    else
        fail "man page missing --fields (cut)"
    fi

    if man ./build/man/modbox-cut.1 2>/dev/null | col -b | grep -q "complement"; then
        pass "man page contains --complement (cut)"
    else
        fail "man page missing --complement (cut)"
    fi

    if man ./build/man/modbox-cut.1 2>/dev/null | col -b | grep -q "delimiter"; then
        pass "man page contains --delimiter (cut)"
    else
        fail "man page missing --delimiter (cut)"
    fi

    # Test that date man page contains key options
    if man ./build/man/modbox-date.1 2>/dev/null | col -b | grep -q "strftime"; then
        pass "man page contains strftime (date)"
    else
        fail "man page missing strftime (date)"
    fi

    if man ./build/man/modbox-date.1 2>/dev/null | col -b | grep -q "RFC"; then
        pass "man page contains RFC format (date)"
    else
        fail "man page missing RFC format (date)"
    fi

    if man ./build/man/modbox-date.1 2>/dev/null | col -b | grep -q "ISO"; then
        pass "man page contains ISO format (date)"
    else
        fail "man page missing ISO format (date)"
    fi

    if man ./build/man/modbox-date.1 2>/dev/null | col -b | grep -q "%Y"; then
        pass "man page contains %Y format (date)"
    else
        fail "man page missing %Y format (date)"
    fi

    if man ./build/man/modbox-date.1 2>/dev/null | col -b | grep -q "%H"; then
        pass "man page contains %H format (date)"
    else
        fail "man page missing %H format (date)"
    fi

    if man ./build/man/modbox-date.1 2>/dev/null | col -b | grep -q "%N"; then
        pass "man page contains %N format (date)"
    else
        fail "man page missing %N format (date)"
    fi

    # Test that uniq man page contains key options
    if man ./build/man/modbox-uniq.1 2>/dev/null | col -b | grep -q "count"; then
        pass "man page contains --count (uniq)"
    else
        fail "man page missing --count (uniq)"
    fi

    if man ./build/man/modbox-uniq.1 2>/dev/null | col -b | grep -q "repeated"; then
        pass "man page contains --repeated (uniq)"
    else
        fail "man page missing --repeated (uniq)"
    fi

    if man ./build/man/modbox-uniq.1 2>/dev/null | col -b | grep -q "unique"; then
        pass "man page contains --unique (uniq)"
    else
        fail "man page missing --unique (uniq)"
    fi

    if man ./build/man/modbox-uniq.1 2>/dev/null | col -b | grep -q "skip-fields"; then
        pass "man page contains --skip-fields (uniq)"
    else
        fail "man page missing --skip-fields (uniq)"
    fi

    # Test that tee man page contains key options
    if man ./build/man/modbox-tee.1 2>/dev/null | col -b | grep -q "append"; then
        pass "man page contains --append (tee)"
    else
        fail "man page missing --append (tee)"
    fi

    if man ./build/man/modbox-tee.1 2>/dev/null | col -b | grep -q "ignore-interrupts"; then
        pass "man page contains --ignore-interrupts (tee)"
    else
        fail "man page missing --ignore-interrupts (tee)"
    fi

    if man ./build/man/modbox-tee.1 2>/dev/null | col -b | grep -q "error-action"; then
        pass "man page contains --error-action (tee)"
    else
        fail "man page missing --error-action (tee)"
    fi

    if man ./build/man/modbox-tee.1 2>/dev/null | col -b | grep -q "warn"; then
        pass "man page contains warn error mode (tee)"
    else
        fail "man page missing warn error mode (tee)"
    fi

    # Test that wc man page contains key options
    if man ./build/man/modbox-wc.1 2>/dev/null | col -b | grep -q "bytes"; then
        pass "man page contains --bytes (wc)"
    else
        fail "man page missing --bytes (wc)"
    fi

    if man ./build/man/modbox-wc.1 2>/dev/null | col -b | grep -q "chars"; then
        pass "man page contains --chars (wc)"
    else
        fail "man page missing --chars (wc)"
    fi

    if man ./build/man/modbox-wc.1 2>/dev/null | col -b | grep -q "lines"; then
        pass "man page contains --lines (wc)"
    else
        fail "man page missing --lines (wc)"
    fi

    if man ./build/man/modbox-wc.1 2>/dev/null | col -b | grep -q "words"; then
        pass "man page contains --words (wc)"
    else
        fail "man page missing --words (wc)"
    fi

    if man ./build/man/modbox-wc.1 2>/dev/null | col -b | grep -q "json"; then
        pass "man page contains --json (wc)"
    else
        fail "man page missing --json (wc)"
    fi

    # Test that mkdir man page contains key options
    if man ./build/man/modbox-mkdir.1 2>/dev/null | col -b | grep -q "parents"; then
        pass "man page contains --parents (mkdir)"
    else
        fail "man page missing --parents (mkdir)"
    fi

    if man ./build/man/modbox-mkdir.1 2>/dev/null | col -b | grep -q "mode"; then
        pass "man page contains --mode (mkdir)"
    else
        fail "man page missing --mode (mkdir)"
    fi

    if man ./build/man/modbox-mkdir.1 2>/dev/null | col -b | grep -q "verbose"; then
        pass "man page contains --verbose (mkdir)"
    else
        fail "man page missing --verbose (mkdir)"
    fi

    # Test that chmod man page contains key options
    if man ./build/man/modbox-chmod.1 2>/dev/null | col -b | grep -q "recursive"; then
        pass "man page contains --recursive (chmod)"
    else
        fail "man page missing --recursive (chmod)"
    fi

    if man ./build/man/modbox-chmod.1 2>/dev/null | col -b | grep -q "reference"; then
        pass "man page contains --reference (chmod)"
    else
        fail "man page missing --reference (chmod)"
    fi

    if man ./build/man/modbox-chmod.1 2>/dev/null | col -b | grep -q "verbose"; then
        pass "man page contains --verbose (chmod)"
    else
        fail "man page missing --verbose (chmod)"
    fi

    # Test that chown man page contains key options
    if man ./build/man/modbox-chown.1 2>/dev/null | col -b | grep -q "recursive"; then
        pass "man page contains --recursive (chown)"
    else
        fail "man page missing --recursive (chown)"
    fi

    if man ./build/man/modbox-chown.1 2>/dev/null | col -b | grep -q "reference"; then
        pass "man page contains --reference (chown)"
    else
        fail "man page missing --reference (chown)"
    fi

    if man ./build/man/modbox-chown.1 2>/dev/null | col -b | grep -q "no-dereference"; then
        pass "man page contains --no-dereference (chown)"
    else
        fail "man page missing --no-dereference (chown)"
    fi

    # Test that ln man page contains key options
    if man ./build/man/modbox-ln.1 2>/dev/null | col -b | grep -q "symbolic"; then
        pass "man page contains --symbolic (ln)"
    else
        fail "man page missing --symbolic (ln)"
    fi

    if man ./build/man/modbox-ln.1 2>/dev/null | col -b | grep -q "force"; then
        pass "man page contains --force (ln)"
    else
        fail "man page missing --force (ln)"
    fi

    if man ./build/man/modbox-ln.1 2>/dev/null | col -b | grep -q "interactive"; then
        pass "man page contains --interactive (ln)"
    else
        fail "man page missing --interactive (ln)"
    fi

    # Test that dd man page contains key options
    if man ./build/man/modbox-dd.1 2>/dev/null | col -b | grep -q "conv"; then
        pass "man page contains conv= (dd)"
    else
        fail "man page missing conv= (dd)"
    fi

    if man ./build/man/modbox-dd.1 2>/dev/null | col -b | grep -q "iflag"; then
        pass "man page contains iflag= (dd)"
    else
        fail "man page missing iflag= (dd)"
    fi

    if man ./build/man/modbox-dd.1 2>/dev/null | col -b | grep -q "status"; then
        pass "man page contains status= (dd)"
    else
        fail "man page missing status= (dd)"
    fi

    # Test that tr man page contains key options
    if man ./build/man/modbox-tr.1 2>/dev/null | col -b | grep -q "complement"; then
        pass "man page contains --complement (tr)"
    else
        fail "man page missing --complement (tr)"
    fi

    if man ./build/man/modbox-tr.1 2>/dev/null | col -b | grep -q "delete"; then
        pass "man page contains --delete (tr)"
    else
        fail "man page missing --delete (tr)"
    fi

    if man ./build/man/modbox-tr.1 2>/dev/null | col -b | grep -q "squeeze"; then
        pass "man page contains --squeeze (tr)"
    else
        fail "man page missing --squeeze (tr)"
    fi

    # Test that uname man page contains key options
    if man ./build/man/modbox-uname.1 2>/dev/null | col -b | grep -q "all"; then
        pass "man page contains --all (uname)"
    else
        fail "man page missing --all (uname)"
    fi

    if man ./build/man/modbox-uname.1 2>/dev/null | col -b | grep -q "kernel"; then
        pass "man page contains --kernel (uname)"
    else
        fail "man page missing --kernel (uname)"
    fi

    if man ./build/man/modbox-uname.1 2>/dev/null | col -b | grep -q "machine"; then
        pass "man page contains --machine (uname)"
    else
        fail "man page missing --machine (uname)"
    fi

    # Test that echo man page contains key options
    if man ./build/man/modbox-echo.1 2>/dev/null | col -b | grep -q "newline"; then
        pass "man page contains -n (echo)"
    else
        fail "man page missing -n (echo)"
    fi

    if man ./build/man/modbox-echo.1 2>/dev/null | col -b | grep -q "backslash"; then
        pass "man page contains -e (echo)"
    else
        fail "man page missing -e (echo)"
    fi

    # Test that xargs man page contains key options
    if man ./build/man/modbox-xargs.1 2>/dev/null | col -b | grep -q "null"; then
        pass "man page contains --null (xargs)"
    else
        fail "man page missing --null (xargs)"
    fi

    if man ./build/man/modbox-xargs.1 2>/dev/null | col -b | grep -q "replace"; then
        pass "man page contains --replace (xargs)"
    else
        fail "man page missing --replace (xargs)"
    fi

    if man ./build/man/modbox-xargs.1 2>/dev/null | col -b | grep -q "max-args"; then
        pass "man page contains --max-args (xargs)"
    else
        fail "man page missing --max-args (xargs)"
    fi

    # Test that basename man page contains key options
    if man ./build/man/modbox-basename.1 2>/dev/null | col -b | grep -q "suffix"; then
        pass "man page contains --suffix (basename)"
    else
        fail "man page missing --suffix (basename)"
    fi

    if man ./build/man/modbox-basename.1 2>/dev/null | col -b | grep -q "multiple"; then
        pass "man page contains --multiple (basename)"
    else
        fail "man page missing --multiple (basename)"
    fi

    if man ./build/man/modbox-basename.1 2>/dev/null | col -b | grep -q "NUL"; then
        pass "man page contains --zero (basename)"
    else
        fail "man page missing --zero (basename)"
    fi

    # Test that dirname man page contains key options
    if man ./build/man/modbox-dirname.1 2>/dev/null | col -b | grep -q "NUL"; then
        pass "man page contains --zero (dirname)"
    else
        fail "man page missing --zero (dirname)"
    fi

    # Test that pwd man page contains key options
    if man ./build/man/modbox-pwd.1 2>/dev/null | col -b | grep -q "logical"; then
        pass "man page contains --logical (pwd)"
    else
        fail "man page missing --logical (pwd)"
    fi

    if man ./build/man/modbox-pwd.1 2>/dev/null | col -b | grep -q "physical"; then
        pass "man page contains --physical (pwd)"
    else
        fail "man page missing --physical (pwd)"
    fi

    # Test that sleep man page contains key options
    if man ./build/man/modbox-sleep.1 2>/dev/null | col -b | grep -q "seconds"; then
        pass "man page contains seconds (sleep)"
    else
        fail "man page missing seconds (sleep)"
    fi

    if man ./build/man/modbox-sleep.1 2>/dev/null | col -b | grep -q "sum"; then
        pass "man page documents argument summation (sleep)"
    else
        fail "man page missing argument summation (sleep)"
    fi

    # Test touch, readlink, mktemp, install, seq, comm, paste, nl man pages
    if man ./build/man/modbox-touch.1 2>/dev/null | col -b | grep -q "date"; then
        pass "man page documents date (touch)"
    else
        fail "man page missing date (touch)"
    fi

    if man ./build/man/modbox-readlink.1 2>/dev/null | col -b | grep -q "symlink"; then
        pass "man page documents symlink (readlink)"
    else
        fail "man page missing symlink (readlink)"
    fi

    if man ./build/man/modbox-mktemp.1 2>/dev/null | col -b | grep -q "template"; then
        pass "man page documents template (mktemp)"
    else
        fail "man page missing template (mktemp)"
    fi

    if man ./build/man/modbox-install.1 2>/dev/null | col -b | grep -q "destination"; then
        pass "man page documents destination (install)"
    else
        fail "man page missing destination (install)"
    fi

    if man ./build/man/modbox-seq.1 2>/dev/null | col -b | grep -q "sequence"; then
        pass "man page documents sequence (seq)"
    else
        fail "man page missing sequence (seq)"
    fi

    if man ./build/man/modbox-comm.1 2>/dev/null | col -b | grep -q "common"; then
        pass "man page documents common lines (comm)"
    else
        fail "man page missing common lines (comm)"
    fi

    if man ./build/man/modbox-paste.1 2>/dev/null | col -b | grep -q "merge"; then
        pass "man page documents merge (paste)"
    else
        fail "man page missing merge (paste)"
    fi

    if man ./build/man/modbox-nl.1 2>/dev/null | col -b | grep -q "number"; then
        pass "man page documents number (nl)"
    else
        fail "man page missing number (nl)"
    fi

    # Test install-man with DESTDIR

    DESTDIR="/tmp/modbox-man-test" PREFIX="/usr" make install-man >/dev/null 2>&1 || true
    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-cat.1.gz" ]]; then
        pass "install-man places modbox-cat.1.gz correctly"
    else
        fail "install-man missing modbox-cat.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-ls.1.gz" ]]; then
        pass "install-man places modbox-ls.1.gz correctly"
    else
        fail "install-man missing modbox-ls.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-rm.1.gz" ]]; then
        pass "install-man places modbox-rm.1.gz correctly"
    else
        fail "install-man missing modbox-rm.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-cp.1.gz" ]]; then
        pass "install-man places modbox-cp.1.gz correctly"
    else
        fail "install-man missing modbox-cp.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-mv.1.gz" ]]; then
        pass "install-man places modbox-mv.1.gz correctly"
    else
        fail "install-man missing modbox-mv.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-rmdir.1.gz" ]]; then
        pass "install-man places modbox-rmdir.1.gz correctly"
    else
        fail "install-man missing modbox-rmdir.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-arch.1.gz" ]]; then
        pass "install-man places modbox-arch.1.gz correctly"
    else
        fail "install-man missing modbox-arch.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-audit2allow.1.gz" ]]; then
        pass "install-man places modbox-audit2allow.1.gz correctly"
    else
        fail "install-man missing modbox-audit2allow.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-awk.1.gz" ]]; then
        pass "install-man places modbox-awk.1.gz correctly"
    else
        fail "install-man missing modbox-awk.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-head.1.gz" ]]; then
        pass "install-man places modbox-head.1.gz correctly"
    else
        fail "install-man missing modbox-head.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-tail.1.gz" ]]; then
        pass "install-man places modbox-tail.1.gz correctly"
    else
        fail "install-man missing modbox-tail.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-sort.1.gz" ]]; then
        pass "install-man places modbox-sort.1.gz correctly"
    else
        fail "install-man missing modbox-sort.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-grep.1.gz" ]]; then
        pass "install-man places modbox-grep.1.gz correctly"
    else
        fail "install-man missing modbox-grep.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-sed.1.gz" ]]; then
        pass "install-man places modbox-sed.1.gz correctly"
    else
        fail "install-man missing modbox-sed.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-find.1.gz" ]]; then
        pass "install-man places modbox-find.1.gz correctly"
    else
        fail "install-man missing modbox-find.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-stat.1.gz" ]]; then
        pass "install-man places modbox-stat.1.gz correctly"
    else
        fail "install-man missing modbox-stat.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-diff.1.gz" ]]; then
        pass "install-man places modbox-diff.1.gz correctly"
    else
        fail "install-man missing modbox-diff.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-cut.1.gz" ]]; then
        pass "install-man places modbox-cut.1.gz correctly"
    else
        fail "install-man missing modbox-cut.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-date.1.gz" ]]; then
        pass "install-man places modbox-date.1.gz correctly"
    else
        fail "install-man missing modbox-date.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-uniq.1.gz" ]]; then
        pass "install-man places modbox-uniq.1.gz correctly"
    else
        fail "install-man missing modbox-uniq.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-tee.1.gz" ]]; then
        pass "install-man places modbox-tee.1.gz correctly"
    else
        fail "install-man missing modbox-tee.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-wc.1.gz" ]]; then
        pass "install-man places modbox-wc.1.gz correctly"
    else
        fail "install-man missing modbox-wc.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-mkdir.1.gz" ]]; then
        pass "install-man places modbox-mkdir.1.gz correctly"
    else
        fail "install-man missing modbox-mkdir.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-chmod.1.gz" ]]; then
        pass "install-man places modbox-chmod.1.gz correctly"
    else
        fail "install-man missing modbox-chmod.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-chown.1.gz" ]]; then
        pass "install-man places modbox-chown.1.gz correctly"
    else
        fail "install-man missing modbox-chown.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-ln.1.gz" ]]; then
        pass "install-man places modbox-ln.1.gz correctly"
    else
        fail "install-man missing modbox-ln.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-dd.1.gz" ]]; then
        pass "install-man places modbox-dd.1.gz correctly"
    else
        fail "install-man missing modbox-dd.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-tr.1.gz" ]]; then
        pass "install-man places modbox-tr.1.gz correctly"
    else
        fail "install-man missing modbox-tr.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-uname.1.gz" ]]; then
        pass "install-man places modbox-uname.1.gz correctly"
    else
        fail "install-man missing modbox-uname.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-echo.1.gz" ]]; then
        pass "install-man places modbox-echo.1.gz correctly"
    else
        fail "install-man missing modbox-echo.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-xargs.1.gz" ]]; then
        pass "install-man places modbox-xargs.1.gz correctly"
    else
        fail "install-man missing modbox-xargs.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-basename.1.gz" ]]; then
        pass "install-man places modbox-basename.1.gz correctly"
    else
        fail "install-man missing modbox-basename.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-dirname.1.gz" ]]; then
        pass "install-man places modbox-dirname.1.gz correctly"
    else
        fail "install-man missing modbox-dirname.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-pwd.1.gz" ]]; then
        pass "install-man places modbox-pwd.1.gz correctly"
    else
        fail "install-man missing modbox-pwd.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-sleep.1.gz" ]]; then
        pass "install-man places modbox-sleep.1.gz correctly"
    else
        fail "install-man missing modbox-sleep.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-touch.1.gz" ]]; then
        pass "install-man places modbox-touch.1.gz correctly"
    else
        fail "install-man missing modbox-touch.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-readlink.1.gz" ]]; then
        pass "install-man places modbox-readlink.1.gz correctly"
    else
        fail "install-man missing modbox-readlink.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-mktemp.1.gz" ]]; then
        pass "install-man places modbox-mktemp.1.gz correctly"
    else
        fail "install-man missing modbox-mktemp.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-install.1.gz" ]]; then
        pass "install-man places modbox-install.1.gz correctly"
    else
        fail "install-man missing modbox-install.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-seq.1.gz" ]]; then
        pass "install-man places modbox-seq.1.gz correctly"
    else
        fail "install-man missing modbox-seq.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-comm.1.gz" ]]; then
        pass "install-man places modbox-comm.1.gz correctly"
    else
        fail "install-man missing modbox-comm.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-paste.1.gz" ]]; then
        pass "install-man places modbox-paste.1.gz correctly"
    else
        fail "install-man missing modbox-paste.1.gz"
    fi

    if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-nl.1.gz" ]]; then
        pass "install-man places modbox-nl.1.gz correctly"
    else
        fail "install-man missing modbox-nl.1.gz"
    fi

    # Verify installed files are gzipped

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-cat.1.gz" | grep -q "gzip compressed data"; then
        pass "installed man page is gzipped"
    else
        fail "installed man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-arch.1.gz" | grep -q "gzip compressed data"; then
        pass "installed arch man page is gzipped"
    else
        fail "installed arch man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-audit2allow.1.gz" | grep -q "gzip compressed data"; then
        pass "installed audit2allow man page is gzipped"
    else
        fail "installed audit2allow man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-awk.1.gz" | grep -q "gzip compressed data"; then
        pass "installed awk man page is gzipped"
    else
        fail "installed awk man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-head.1.gz" | grep -q "gzip compressed data"; then
        pass "installed head man page is gzipped"
    else
        fail "installed head man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-tail.1.gz" | grep -q "gzip compressed data"; then
        pass "installed tail man page is gzipped"
    else
        fail "installed tail man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-sort.1.gz" | grep -q "gzip compressed data"; then
        pass "installed sort man page is gzipped"
    else
        fail "installed sort man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-grep.1.gz" | grep -q "gzip compressed data"; then
        pass "installed grep man page is gzipped"
    else
        fail "installed grep man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-sed.1.gz" | grep -q "gzip compressed data"; then
        pass "installed sed man page is gzipped"
    else
        fail "installed sed man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-find.1.gz" | grep -q "gzip compressed data"; then
        pass "installed find man page is gzipped"
    else
        fail "installed find man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-stat.1.gz" | grep -q "gzip compressed data"; then
        pass "installed stat man page is gzipped"
    else
        fail "installed stat man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-diff.1.gz" | grep -q "gzip compressed data"; then
        pass "installed diff man page is gzipped"
    else
        fail "installed diff man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-cut.1.gz" | grep -q "gzip compressed data"; then
        pass "installed cut man page is gzipped"
    else
        fail "installed cut man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-date.1.gz" | grep -q "gzip compressed data"; then
        pass "installed date man page is gzipped"
    else
        fail "installed date man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-uniq.1.gz" | grep -q "gzip compressed data"; then
        pass "installed uniq man page is gzipped"
    else
        fail "installed uniq man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-tee.1.gz" | grep -q "gzip compressed data"; then
        pass "installed tee man page is gzipped"
    else
        fail "installed tee man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-wc.1.gz" | grep -q "gzip compressed data"; then
        pass "installed wc man page is gzipped"
    else
        fail "installed wc man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-mkdir.1.gz" | grep -q "gzip compressed data"; then
        pass "installed mkdir man page is gzipped"
    else
        fail "installed mkdir man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-chmod.1.gz" | grep -q "gzip compressed data"; then
        pass "installed chmod man page is gzipped"
    else
        fail "installed chmod man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-chown.1.gz" | grep -q "gzip compressed data"; then
        pass "installed chown man page is gzipped"
    else
        fail "installed chown man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-ln.1.gz" | grep -q "gzip compressed data"; then
        pass "installed ln man page is gzipped"
    else
        fail "installed ln man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-dd.1.gz" | grep -q "gzip compressed data"; then
        pass "installed dd man page is gzipped"
    else
        fail "installed dd man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-tr.1.gz" | grep -q "gzip compressed data"; then
        pass "installed tr man page is gzipped"
    else
        fail "installed tr man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-uname.1.gz" | grep -q "gzip compressed data"; then
        pass "installed uname man page is gzipped"
    else
        fail "installed uname man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-echo.1.gz" | grep -q "gzip compressed data"; then
        pass "installed echo man page is gzipped"
    else
        fail "installed echo man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-xargs.1.gz" | grep -q "gzip compressed data"; then
        pass "installed xargs man page is gzipped"
    else
        fail "installed xargs man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-basename.1.gz" | grep -q "gzip compressed data"; then
        pass "installed basename man page is gzipped"
    else
        fail "installed basename man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-dirname.1.gz" | grep -q "gzip compressed data"; then
        pass "installed dirname man page is gzipped"
    else
        fail "installed dirname man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-pwd.1.gz" | grep -q "gzip compressed data"; then
        pass "installed pwd man page is gzipped"
    else
        fail "installed pwd man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-sleep.1.gz" | grep -q "gzip compressed data"; then
        pass "installed sleep man page is gzipped"
    else
        fail "installed sleep man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-touch.1.gz" | grep -q "gzip compressed data"; then
        pass "installed touch man page is gzipped"
    else
        fail "installed touch man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-readlink.1.gz" | grep -q "gzip compressed data"; then
        pass "installed readlink man page is gzipped"
    else
        fail "installed readlink man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-mktemp.1.gz" | grep -q "gzip compressed data"; then
        pass "installed mktemp man page is gzipped"
    else
        fail "installed mktemp man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-install.1.gz" | grep -q "gzip compressed data"; then
        pass "installed install man page is gzipped"
    else
        fail "installed install man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-seq.1.gz" | grep -q "gzip compressed data"; then
        pass "installed seq man page is gzipped"
    else
        fail "installed seq man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-comm.1.gz" | grep -q "gzip compressed data"; then
        pass "installed comm man page is gzipped"
    else
        fail "installed comm man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-paste.1.gz" | grep -q "gzip compressed data"; then
        pass "installed paste man page is gzipped"
    else
        fail "installed paste man page is not gzipped"
    fi

    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-nl.1.gz" | grep -q "gzip compressed data"; then
        pass "installed nl man page is gzipped"
    else
        fail "installed nl man page is not gzipped"
    fi

    # Test checksum & encoding batch: md5sum, sha*, b2sum, base32, base64, basenc, cksum, sum
    for cmd in md5sum sha1sum sha224sum sha256sum sha384sum sha512sum b2sum base32 base64 basenc cksum sum; do
        if [[ -f "docs/man/modbox-$cmd.1.md" ]]; then
            pass "docs/man/modbox-$cmd.1.md exists"
        else
            fail "docs/man/modbox-$cmd.1.md missing"
        fi
        if [[ -f "build/man/modbox-$cmd.1" ]]; then
            pass "build/man/modbox-$cmd.1 generated"
        else
            fail "build/man/modbox-$cmd.1 not generated"
        fi
        if man "./build/man/modbox-$cmd.1" 2>/dev/null | col -b | grep -qi "modbox-$cmd"; then
            pass "man page $cmd renders with its identity"
        else
            fail "man page $cmd missing identity in rendered output"
        fi
        # Representative option/heading sanity check per command
        case "$cmd" in
            md5sum|sha1sum|sha224sum|sha256sum|sha384sum|sha512sum) tok="--check" ;;
            b2sum) tok="--length" ;;
            base32|base64) tok="--decode" ;;
            basenc) tok="ENCODING" ;;
            cksum) tok="--verbose" ;;
            sum) tok="--sysv" ;;
            *) tok="" ;;
        esac
        # Note: pandoc's man writer renders a bolded leading "--" as an en dash,
        # so we assert the option against the authored source .md instead.
        if [[ -n "$tok" ]] && grep -q -- "$tok" "docs/man/modbox-$cmd.1.md"; then
            pass "man page source $cmd documents $tok"
        else
            fail "man page source $cmd missing $tok"
        fi

        if [[ -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-$cmd.1.gz" ]]; then

            pass "install-man places modbox-$cmd.1.gz correctly"
        else
            fail "install-man missing modbox-$cmd.1.gz"
        fi
        if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-$cmd.1.gz" | grep -q "gzip compressed data"; then
            pass "installed $cmd man page is gzipped"
        else
            fail "installed $cmd man page is not gzipped"
        fi
    done

    # Test uninstall-man

    DESTDIR="/tmp/modbox-man-test" PREFIX="/usr" make uninstall-man >/dev/null 2>&1 || true
    if [[ ! -f "/tmp/modbox-man-test/usr/share/man/man1/modbox-cat.1.gz" ]]; then
        pass "uninstall-man removes installed files"
    else
        fail "uninstall-man did not remove installed files"
    fi

    # Clean up test directory

    rm -rf "/tmp/modbox-man-test"


else
    echo "  SKIP  pandoc not installed; skipping content and install tests"
fi

echo ""
echo "=== Man Page Tests Complete ==="