#!/usr/bin/env bash
#
# test_man_issue62.sh — Man-page coverage checks for issue #62
#
# Verifies that chattr, chcon, chgrp, and chroot each have a pandoc man source
# under docs/man/, that each source is registered in MAN_SOURCES in the
# Makefile, that `make man` renders each one, and that each source contains the
# required section headings (NAME, SYNOPSIS, OPTIONS, EXIT STATUS).

# shellcheck source=framework.sh
source "$(dirname "${BASH_SOURCE[0]}")/framework.sh"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAN_SRC_DIR="$PROJECT_ROOT/docs/man"
MAKEFILE="$PROJECT_ROOT/Makefile"

CMDS=(chattr chcon chgrp chroot)
REQUIRED_HEADINGS=("NAME" "SYNOPSIS" "OPTIONS" "EXIT STATUS")

# Render the man pages once for all checks below.
make -C "$PROJECT_ROOT" man >/dev/null 2>&1

for cmd in "${CMDS[@]}"; do
    src="$MAN_SRC_DIR/modbox-$cmd.1.md"
    rendered="$PROJECT_ROOT/build/man/modbox-$cmd.1"

    # 1. Source file exists
    if [[ -f "$src" ]]; then
        pass "docs/man/modbox-$cmd.1.md exists"
    else
        fail "docs/man/modbox-$cmd.1.md missing"
        continue
    fi

    # 2. Registered in MAN_SOURCES
    if grep -q "modbox-$cmd.1.md" "$MAKEFILE"; then
        pass "modbox-$cmd.1.md listed in MAN_SOURCES"
    else
        fail "modbox-$cmd.1.md not registered in MAN_SOURCES"
    fi

    # 3. Rendered by `make man`
    if [[ -f "$rendered" ]]; then
        pass "make man renders modbox-$cmd"
    else
        fail "make man did not produce build/man/modbox-$cmd.1"
    fi

    # 4. Required section headings present
    for heading in "${REQUIRED_HEADINGS[@]}"; do
        if grep -q "^# ${heading}\$" "$src"; then
            pass "modbox-$cmd.1.md has # $heading section"
        else
            fail "modbox-$cmd.1.md missing # $heading section"
        fi
    done
done
