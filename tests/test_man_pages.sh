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

if grep -q "modbox-cat.1.md" Makefile && grep -q "modbox-ls.1.md" Makefile && grep -q "modbox-rm.1.md" Makefile && grep -q "modbox-cp.1.md" Makefile && grep -q "modbox-mv.1.md" Makefile && grep -q "modbox-rmdir.1.md" Makefile && grep -q "modbox-arch.1.md" Makefile && grep -q "modbox-audit2allow.1.md" Makefile && grep -q "modbox-awk.1.md" Makefile; then
    pass "Makefile lists all man page sources"
else
    fail "Makefile missing man page sources"
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

    if man ./build/man/modbox-ls.1 2>/dev/null | col -b | grep -q "recursive"; then
        pass "man page contains -r/--recursive"
    else
        fail "man page missing -r/--recursive"
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