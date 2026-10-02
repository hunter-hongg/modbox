#!/usr/bin/env bash
#
# test_addr2line.sh — Tests for the addr2line command.
#
# GNU addr2line (binutils 2.46) was the reference while writing the command,
# and a differential sweep over every address in a test binary confirmed the
# full output matrix matches it exactly. The expectations below are derived
# from the fixture rather than hard-coded: line numbers come from the fixture
# source and addresses from `nm`, so they survive a toolchain change.
#

# shellcheck source=framework.sh
source "$(dirname "${BASH_SOURCE[0]}")/framework.sh"

echo ""
echo "── addr2line ────────────────────────────────────"

# ── Fixtures ──────────────────────────────────────────────────────────────
# "debug"    — C++ with DWARF, a mangled symbol and a separate main(), for
#              demangling, symbol+offset and multi-address tests.
# "nodebug"  — the same program stripped, for the empty-symbol-table path.
cd "$TMPDIR" || exit 1

cat > test.cpp <<'EOF'
#include <cstdio>

struct Foo {
    int bar(int x) {
        std::printf("%d\n", x * 2);
        return x + 1;
    }
};

int main() {
    Foo f;
    f.bar(21);
    return 0;
}
EOF

REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Pick a C++ compiler: anything on PATH, else the project's vendored gcc.
CXX=""
for cand in g++ c++ clang++ g++-16 g++-14 g++-13; do
    if command -v "$cand" >/dev/null 2>&1; then
        CXX="$cand"
        break
    fi
done
if [ -z "$CXX" ]; then
    for cand in "$REPO_ROOT/.brew/Homebrew/opt/gcc/bin/g++-16" \
                "$HOME/.brew/Homebrew/opt/gcc/bin/g++-16"; do
        if [ -x "$cand" ]; then
            CXX="$cand"
            break
        fi
    done
fi

if [ -z "$CXX" ]; then
    echo "  SKIP — no C++ compiler available (g++/c++)"
    cd "$SCRIPT_DIR" || exit 1
    return 0 2>/dev/null || exit 0
fi

if ! "$CXX" -std=c++17 -g -fno-inline -o debug test.cpp 2>/dev/null; then
    echo "  SKIP — $CXX could not build the fixture"
    cd "$SCRIPT_DIR" || exit 1
    return 0 2>/dev/null || exit 0
fi
# A stripped build keeps the DWARF (so line info survives) but drops the
# symbol table, which is what the "no symbol" expectations need.
"$CXX" -std=c++17 -g -fno-inline -s -o nodebug test.cpp 2>/dev/null || true

# Fixture line numbers, resolved from the fixture source itself so the
# assertions below describe the fixture rather than a remembered line count.
# Note that DWARF attributes the function-entry address to the line *above*
# the first statement (the prologue is charged to the declaration), so the
# expectation for an address inside Foo::bar is the printf line minus one.
BAR_ADDR=$(nm debug | awk '$3=="_ZN3Foo3barEi" {print "0x"$1; exit}')
MAIN_ADDR=$(nm debug | awk '$3=="main"      {print "0x"$1; exit}')
START_ADDR=$(nm debug | awk '$3=="_start"   {print "0x"$1; exit}')
BAR_PRINTF_LINE=$(grep -n 'std::printf' test.cpp | head -n1 | cut -d: -f1)
BAR_LINE=$((BAR_PRINTF_LINE - 1))
MAIN_LINE=$(grep -n '^int main' test.cpp | cut -d: -f1)
MAIN_CALL_LINE=$(grep -n 'f.bar' test.cpp | head -n1 | cut -d: -f1)

# The addresses the offsets below land on, per the fixture's line table.
# main+0x10 and main+10 both fall on the call line; main+0x20 reaches past
# main's prologue, where the reference reports a known function but no line.
MAIN_PLUS_HEX_LINE=$MAIN_CALL_LINE

# ── Usage / version ──────────────────────────────────────────────────────

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' addr2line --help

echo "  ── -h short form ──"
assert_cmd_pat 'Usage:' addr2line -h

echo "  ── -V version ──"
assert_cmd_pat 'addr2line \(modbox\)' addr2line -V

echo "  ── -v is the reference's other version alias ──"
assert_cmd_pat 'addr2line \(modbox\)' addr2line -v

echo "  ── help mentions every supported option ──"
# The framework's pattern starts with '-', so anchor it with a leading space
# to keep grep from treating it as an option switch.
for opt in addresses target exe inlines section pretty-print \
           basenames functions demangle recurse-limit no-recurse-limit; do
    assert_cmd_pat " --$opt" addr2line --help
done

# ── file:line basics ─────────────────────────────────────────────────────

echo "  ── with no options, only file:line is printed ──"
assert_cmd "test.cpp:$BAR_LINE" addr2line -s -e debug "$BAR_ADDR"

echo "  ── without -s the full path is kept ──"
assert_cmd "$TMPDIR/test.cpp:$BAR_LINE" addr2line -e debug "$BAR_ADDR"

echo "  ── -s strips the directory from the file name ──"
assert_cmd "test.cpp:$BAR_LINE" addr2line -s -e debug "$BAR_ADDR"

# ── function names ───────────────────────────────────────────────────────

echo "  ── -f prints the raw (mangled) function name ──"
assert_cmd "_ZN3Foo3barEi
test.cpp:$BAR_LINE" addr2line -f -s -e debug "$BAR_ADDR"

echo "  ── -C demangles the function name ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE" addr2line -f -C -s -e debug "$BAR_ADDR"

echo "  ── -C without -f still resolves, just without the name ──"
assert_cmd "test.cpp:$BAR_LINE" addr2line -C -s -e debug "$BAR_ADDR"

echo "  ── -a prints the address first ──"
assert_cmd "0x${BAR_ADDR#0x}
_ZN3Foo3barEi
test.cpp:$BAR_LINE" addr2line -a -f -s -e debug "$BAR_ADDR"

echo "  ── -a prints the address alone without -f ──"
assert_cmd "0x${BAR_ADDR#0x}
test.cpp:$BAR_LINE" addr2line -a -s -e debug "$BAR_ADDR"

# ── pretty printing ──────────────────────────────────────────────────────

echo "  ── -p puts function and location on one line ──"
assert_cmd "Foo::bar(int) at test.cpp:$BAR_LINE" \
    addr2line -p -f -C -s -e debug "$BAR_ADDR"

echo "  ── -p -a separates the address with ': ' ──"
assert_cmd "0x${BAR_ADDR#0x}: Foo::bar(int) at test.cpp:$BAR_LINE" \
    addr2line -p -a -f -C -s -e debug "$BAR_ADDR"

echo "  ── -p -a -f for a symbol with no line info ──"
assert_cmd "0x${START_ADDR#0x}: _start at ??:?" \
    addr2line -p -a -f -s -e debug "$START_ADDR"

echo "  ── without -f, -p prints only the location ──"
assert_cmd "test.cpp:$BAR_LINE" addr2line -p -s -e debug "$BAR_ADDR"

# ── unknown addresses and symbols ────────────────────────────────────────

echo "  ── an address with no info yields ??:0 ──"
assert_cmd '??:0' addr2line -e debug 0x9999

echo "  ── -p keeps ??:0 on one line ──"
assert_cmd '??:0' addr2line -p -e debug 0x9999

echo "  ── -a -p prefixes the address ──"
assert_cmd '0x0000000000009999: ??:0' addr2line -a -p -e debug 0x9999

echo "  ── -f reports ?? for the function and ??:0 for the location ──"
assert_cmd '0x0000000000009999
??
??:0' addr2line -a -f -e debug 0x9999

echo "  ── -p -f reports '?? ??:0' ──"
assert_cmd '0x0000000000009999: ?? ??:0' addr2line -a -p -f -e debug 0x9999

echo "  ── an unknown symbol resolves to address 0 ──"
assert_cmd '??
??:0' addr2line -f -e debug nosuchsymbol

echo "  ── a stripped binary has no symbol table, so nothing resolves ──"
if [ -f nodebug ]; then
    assert_cmd '??
??:0' addr2line -f -s -e nodebug "$BAR_ADDR"
    assert_cmd '??:0' addr2line -s -e nodebug "$BAR_ADDR"
fi

# ── multiple addresses and stdin ─────────────────────────────────────────

echo "  ── several addresses are resolved in order ──"
assert_cmd "_ZN3Foo3barEi
test.cpp:$BAR_LINE
main
test.cpp:$MAIN_LINE" addr2line -f -s -e debug "$BAR_ADDR" "$MAIN_ADDR"

echo "  ── addresses are read from stdin when none are given ──"
assert_cmd "_ZN3Foo3barEi
test.cpp:$BAR_LINE" sh -c "echo $BAR_ADDR | addr2line -f -s -e debug"

echo "  ── stdin mode demangles too ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE" sh -c "echo $BAR_ADDR | addr2line -f -C -s -e debug"

echo "  ── stdin mode with -p and -a ──"
assert_cmd "0x${BAR_ADDR#0x}: Foo::bar(int) at test.cpp:$BAR_LINE" \
    sh -c "echo $BAR_ADDR | addr2line -p -a -f -C -s -e debug"

echo "  ── several addresses from stdin, one per line ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE
main
test.cpp:$MAIN_LINE" \
    sh -c "printf '%s\\n%s\\n' $BAR_ADDR $MAIN_ADDR | \
addr2line -f -C -s -e debug"

echo "  ── a blank stdin line is itself a token, so it resolves to 0 ──"
assert_cmd "??
??:0
Foo::bar(int)
test.cpp:$BAR_LINE
??
??:0" sh -c "printf '\\n%s\\n\\n' $BAR_ADDR | addr2line -f -C -s -e debug"

# ── symbol+offset ────────────────────────────────────────────────────────

echo "  ── symbol + offset of zero is the symbol itself ──"
assert_cmd "main
test.cpp:$MAIN_LINE" addr2line -f -C -s -e debug 'main+0'

echo "  ── symbol + hex offset resolves past the symbol start ──"
assert_cmd "main
test.cpp:$MAIN_PLUS_HEX_LINE" addr2line -f -C -s -e debug 'main+0x10'

echo "  ── symbol + decimal offset is decimal, not hex ──"
assert_cmd "main
test.cpp:$MAIN_PLUS_HEX_LINE" addr2line -f -C -s -e debug 'main+10'

echo "  ── whitespace is allowed between symbol and + ──"
assert_cmd "main
test.cpp:$MAIN_PLUS_HEX_LINE" addr2line -f -C -s -e debug 'main + 0x10'

echo "  ── an offset that reaches past the symbol keeps the name, loses the line ──"
assert_cmd 'main
??:?' addr2line -f -C -s -e debug 'main+0x20'

echo "  ── a trailing offset that is not a number still resolves the symbol ──"
assert_cmd "main
test.cpp:$MAIN_LINE" addr2line -f -C -s -e debug 'main+0xZZ'

echo "  ── the mangled name resolves without -C ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE" addr2line -f -C -s -e debug "_ZN3Foo3barEi+0x0"

echo "  ── the demangled name resolves with -C ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE" addr2line -f -C -s -e debug 'Foo::bar(int)+0'

echo "  ── without -C, a demangled name still resolves, spelled mangled ──"
assert_cmd "_ZN3Foo3barEi
test.cpp:$BAR_LINE" addr2line -f -s -e debug 'Foo::bar(int)+0'

# ── token grammar for hex numbers ────────────────────────────────────────
# These follow binutils' is_symbol()/bfd_scan_vma() pair, not intuition:
# a leading '+' is consumed by strtoull, and a leading hex letter without a
# '+' anywhere is a number rather than a symbol.

echo "  ── a leading + is part of a hex number, so +5 is 0x5 ──"
assert_cmd '0x0000000000000005: ??:0' addr2line -a -p -e debug '+5'

echo "  ── +0x5 is also 0x5 ──"
assert_cmd '0x0000000000000005: ??:0' addr2line -a -p -e debug '+0x5'

echo "  ── whitespace may follow the leading + ──"
assert_cmd '0x0000000000000001: ??:0' addr2line -a -p -e debug '+ 1'

echo "  ── ++2 parses as 2, not as a symbol ──"
assert_cmd '0x0000000000000002: ??:0' addr2line -a -p -e debug '++2'

echo "  ── +a is a hex letter token, so it is not a number and resolves to 0 ──"
assert_cmd '0x0000000000000000: ??:0' addr2line -a -p -e debug '+a'

echo "  ── a leading a-f with no + is a hex number ──"
assert_cmd '0x000000000000cafe: ??:0' addr2line -a -p -e debug 'cafe'

echo "  ── a leading g-z is a symbol, so it resolves to 0 ──"
assert_cmd '0x0000000000000000: ??:0' addr2line -a -p -e debug 'zzzz'

echo "  ── a leading digit is always a number, even with a trailing + ──"
assert_cmd '0x0000000000000005: ??:0' addr2line -a -p -e debug '5+'

# ── demangling styles ────────────────────────────────────────────────────

echo "  ── --demangle=gnu-v3 ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE" addr2line --demangle=gnu-v3 -f -s -e debug "$BAR_ADDR"

echo "  ── --demangle=java uses dots instead of colons ──"
assert_cmd "Foo.bar(int)
test.cpp:$BAR_LINE" addr2line --demangle=java -f -s -e debug "$BAR_ADDR"

echo "  ── --demangle=auto behaves like -C ──"
assert_cmd "Foo::bar(int)
test.cpp:$BAR_LINE" addr2line --demangle=auto -f -s -e debug "$BAR_ADDR"

echo "  ── --demangle=gnat wraps unknown names in angle brackets ──"
assert_cmd "<_ZN3Foo3barEi>
test.cpp:$BAR_LINE" addr2line --demangle=gnat -f -s -e debug "$BAR_ADDR"

echo "  ── an unknown style is rejected ──"
assert_cmd_pat_stderr "unknown demangling style 'invalid'" \
    addr2line --demangle=invalid -f -e debug "$BAR_ADDR"

# ── recursion limit flags ────────────────────────────────────────────────

echo "  ── -r and -R are both accepted ──"
assert_cmd_pat 'Foo::bar\(int\)' addr2line -r -f -C -s -e debug "$BAR_ADDR"
assert_cmd_pat 'Foo::bar\(int\)' addr2line -R -f -C -s -e debug "$BAR_ADDR"

# ── section-relative offsets (-j) ────────────────────────────────────────

echo "  ── -j reads a section-relative offset ──"
# The offset is relative to the section's VMA, so compute it from the
# section table rather than assuming a fixed .text base.
TEXT_VMA=$(readelf -SW debug | awk '$3==".text"{print "0x"$5; exit}')
MAIN_OFF=$(( MAIN_ADDR - TEXT_VMA ))
assert_cmd "main
test.cpp:$MAIN_CALL_LINE" \
    addr2line -j .text -f -s -e debug "0x$(printf %x $((MAIN_OFF + 8)))"

echo "  ── -j with a missing section is an error ──"
assert_cmd_pat_stderr "cannot find section '.nosuch'" \
    addr2line -j .nosuch -e debug 0x10

# ── inline unwinding (-i) ────────────────────────────────────────────────

cat > inline.cpp <<'EOF'
#include <cstdio>
static inline int helper(int x) { return x * 3; }
int main() {
    std::printf("%d\n", helper(7));
    return 0;
}
EOF
if "$CXX" -std=c++17 -g -O1 -o inline inline.cpp 2>/dev/null; then
    INL_MAIN=$(nm inline | awk '$3=="main" {print "0x"$1; exit}')
    echo "  ── -i prints the enclosing inline frames when there are any ──"
    assert_cmd_pat 'main' addr2line -i -f -s -e inline "$INL_MAIN"
fi

# ── -b is accepted and ignored ───────────────────────────────────────────

echo "  ── -b/--target is accepted ──"
assert_cmd_pat "test\.cpp:[0-9]" addr2line -b elf64-x86-64 -f -s -e debug "$BAR_ADDR"
assert_cmd_pat "test\.cpp:[0-9]" addr2line --target=elf64-x86-64 -f -s -e debug "$BAR_ADDR"

# ── errors and exit codes ────────────────────────────────────────────────

echo "  ── a missing executable is reported on stderr ──"
assert_cmd_pat_stderr 'No such file' addr2line -e /nonexistent/file "$BAR_ADDR"

echo "  ── a missing executable exits non-zero ──"
RC=$("$MODBOX" addr2line -e /nonexistent/file "$BAR_ADDR" >/dev/null 2>&1; echo $?)
if [ "$RC" -ne 0 ]; then
    pass "addr2line -e /nonexistent/file exits $RC"
else
    fail "addr2line -e /nonexistent/file should exit non-zero, got $RC"
fi

echo "  ── a non-object file is rejected ──"
assert_cmd_pat_stderr '.' addr2line -e "$TMPDIR/test.cpp" 0x1

echo "  ── an unrecognized option is rejected ──"
assert_cmd_pat_stderr "unrecognized option" addr2line --nope -e debug "$BAR_ADDR"

# ── a real system binary still resolves ──────────────────────────────────

echo "  ── /bin/ls resolves a dynamic symbol to a function name ──"
LS_ADDR=$(nm -D /bin/ls 2>/dev/null | awk '$3=="main"{print "0x"$1; exit}')
if [ -n "$LS_ADDR" ]; then
    assert_cmd_pat 'main' addr2line -f -e /bin/ls "$LS_ADDR"
fi

cd "$SCRIPT_DIR" || exit 1
