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

if grep -q "modbox-cat.1.md" Makefile && grep -q "modbox-ls.1.md" Makefile && grep -q "modbox-rm.1.md" Makefile; then
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

    # Verify installed files are gzipped
    if file "/tmp/modbox-man-test/usr/share/man/man1/modbox-cat.1.gz" | grep -q "gzip compressed data"; then
        pass "installed man page is gzipped"
    else
        fail "installed man page is not gzipped"
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