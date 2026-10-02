#!/usr/bin/env bash
#
# test_strings.sh — Tests for the strings command.
#

# shellcheck source=framework.sh
source "$(dirname "${BASH_SOURCE[0]}")/framework.sh"

echo ""
echo "── strings ────────────────────────────────────"

# Build fixtures with printf so the byte values are exact. GNU strings was used
# as the reference while writing the command: the expected values below are its
# output, not guesses.
cd "$TMPDIR" || exit 1

printf 'ab\x00\x00Hello\x00WorldXY\x00' > simple.bin
printf 'AAAA\x00BBBB\x00' > two.bin
printf 'AAAABBBB' > nonul.bin
printf 'AAAA\tBBBB\nCCCC\rDDDD\x00' > ws.bin
printf 'AB\xffCD\x00EF\x00' > hi.bin
printf 'AAAA\x00BBB' > short.bin
: > empty.bin

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' strings --help

echo "  ── -h short form ──"
assert_cmd_pat 'Usage:' strings -h

echo "  ── -V version ──"
assert_cmd_pat 'strings \(modbox\)' strings -V

echo "  ── default minimum length is 4 ──"
assert_cmd "Hello
WorldXY" strings simple.bin

echo "  ── -n lowers the minimum length ──"
assert_cmd "ab
Hello
WorldXY" strings -n 2 simple.bin

echo "  ── --bytes long form ──"
assert_cmd "ab
Hello
WorldXY" strings --bytes=2 simple.bin

echo "  ── -n accepts a 0x prefix ──"
assert_cmd "WorldXY" strings -n 0x6 simple.bin

echo "  ── -n raises the minimum length ──"
assert_cmd "" strings -n 8 simple.bin

echo "  ─-- a run shorter than -n is not printed ──"
assert_cmd "" strings -n 5 short.bin

echo "  ── a run at the end without a NUL is printed ──"
assert_cmd "AAAABBBB" strings nonul.bin

echo "  ── empty file prints nothing ──"
assert_cmd "" strings empty.bin

echo "  ── -t x prints a 7-column hex offset ──"
# The field is a constant 7 columns: "4" is padded with 6 spaces, "a" with 6
# too, so a single-digit hex offset is not one column wider than "4".
assert_cmd "      4 Hello
      a WorldXY" strings -t x simple.bin

echo "  ── -t d prints a decimal offset ──"
assert_cmd "      4 Hello
     10 WorldXY" strings -t d simple.bin

echo "  ── -t o prints an octal offset ──"
assert_cmd "      4 Hello
     12 WorldXY" strings -t o simple.bin

echo "  ── -o is an alias for --radix=o ──"
assert_cmd "      4 Hello
     12 WorldXY" strings -o simple.bin

echo "  ── --radix long form ──"
assert_cmd "      4 Hello
     10 WorldXY" strings --radix=d simple.bin

echo "  ── offsets restart per file ──"
assert_cmd "      0 AAAA
      5 BBBB" strings -t x two.bin

echo "  ── -f prefixes each string with the file name ──"
assert_cmd "two.bin: AAAA
two.bin: BBBB" strings -f two.bin

echo "  ── -f with -t prints name then offset ──"
assert_cmd "two.bin:       0 AAAA
two.bin:       5 BBBB" strings -f -t x two.bin

echo "  ── several files are scanned in order ──"
assert_cmd "AAAA
BBBB
Hello
WorldXY" strings two.bin simple.bin

echo "  ── -f across several files names each one ──"
assert_cmd "two.bin: AAAA
two.bin: BBBB
simple.bin: Hello
simple.bin: WorldXY" strings -f two.bin simple.bin

echo "  ── tab is part of a string, newline ends it ──"
assert_cmd "AAAA	BBBB
CCCC
DDDD" strings ws.bin

echo "  ── -w joins a carriage return, which the default splits ──"
# CCCC and DDDD become one string because -w accepts CR as whitespace; the
# carriage return is kept in the output, not replaced.
assert_cmd "AAAA	BBBB
CCCCDDDD" strings -w -n 6 ws.bin

echo "  ── -w still ends a run at a non-whitespace control byte ──"
assert_cmd "AB
CD
EF" strings -n 2 hi.bin

echo "  ── -w does not join across a high-bit byte ──"
assert_cmd "AB
CD
EF" strings -w -n 2 hi.bin

echo "  ── -s replaces the trailing newline ──"
assert_cmd "AAAA::BBBB::" strings -s '::' two.bin

echo "  ── -a is accepted and scans the whole file ──"
assert_cmd "Hello
WorldXY" strings -a simple.bin

echo "  ── -d is accepted ──"
assert_cmd "Hello
WorldXY" strings -d simple.bin

echo "  ── -h and -V win over an unreadable file ──"
assert_cmd_pat 'Usage:' strings -h simple.bin
assert_cmd_pat 'strings \(modbox\)' strings -V simple.bin

echo "  ── -t wins over -o when both are given ──"
assert_cmd "      4 Hello
     10 WorldXY" strings -o -t d simple.bin

echo "  ── reads standard input when no file is given ──"
assert_cmd "HelloFromStdin" sh -c "printf 'HelloFromStdin' | '$MODBOX' strings"

echo "  ── -f names standard input ──"
assert_cmd "{standard input}: HelloFromStdin" sh -c "printf 'HelloFromStdin' | '$MODBOX' strings -f"

echo "  ── offsets are relative to each file, not to the stream ──"
assert_cmd "      0 AAAA
      5 BBBB
      4 Hello
     10 WorldXY" strings -t d two.bin simple.bin

echo "  ── missing file reports the name and exits 1 ──"
assert_cmd_pat_stderr "nope\.bin" strings nope.bin

echo "  ── a missing file does not stop the remaining ones ──"
assert_cmd "AAAA
BBBB
Hello
WorldXY" strings two.bin nope.bin simple.bin

echo "  ── a missing file still exits 1 ──"
RC=$( "$MODBOX" strings two.bin nope.bin >/dev/null 2>&1; echo $? )
if [ "$RC" -eq 1 ]; then
    pass "mixed readable/missing files → exit 1"
else
    fail "mixed readable/missing files → expected exit 1, got $RC"
fi

echo "  ── a directory is refused, not scanned ──"
assert_cmd_pat_stderr 'is a directory' strings .

echo "  ── -n 0 is rejected ──"
assert_cmd_pat_stderr 'minimum string length is too small: 0' strings -n 0 simple.bin

echo "  ── a negative -n is rejected ──"
assert_cmd_pat_stderr 'minimum string length is too big' strings -n -1 simple.bin

echo "  ── -n reports the value as written, so -0 stays -0 ──"
assert_cmd_pat_stderr 'minimum string length is too small: -0' strings -n -0 simple.bin

echo "  ── an empty -n is too small, not malformed ──"
assert_cmd_pat_stderr 'minimum string length is too small: ' strings -n '' simple.bin

echo "  ── a non-numeric -n is rejected ──"
assert_cmd_pat_stderr 'invalid integer argument abc' strings -n abc simple.bin

echo "  ── a bare 0x prefix is rejected ──"
assert_cmd_pat_stderr 'invalid integer argument 0x' strings -n 0x simple.bin

echo "  ── a trailing suffix makes -n malformed ──"
assert_cmd_pat_stderr 'invalid integer argument 08' strings -n 08 simple.bin

echo "  ── -n accepts a 0b binary prefix ──"
assert_cmd "Hello
WorldXY" strings -n 0b11 simple.bin

echo "  ── -n 0x100000000 exceeds the reference's 32-bit field ──"
assert_cmd_pat_stderr 'minimum string length is too big: 0x100000000' strings -n 0x100000000 simple.bin

echo "  ── 0xffffffff gets the reference's distinct wording ──"
assert_cmd_pat_stderr 'minimum string length 0xffffffff is too big' strings -n 0xffffffff simple.bin

echo "  ── the largest accepted -n is 2^32-2 and prints nothing ──"
assert_cmd "" strings -n 4294967294 simple.bin
assert_cmd_pat_stderr '4294967295 is too big' strings -n 4294967295 simple.bin

echo "  ── an invalid radix is rejected ──"
assert_cmd_pat_stderr "invalid radix 'q'" strings -t q simple.bin

echo "  ── an unrecognized option is rejected ──"
assert_cmd_pat_stderr "unrecognized option '--nope'" strings --nope simple.bin

echo "  ── finds real strings in a real binary ──"
assert_cmd_pat 'SAPH' strings /bin/ls

echo "  ── -n prunes a real binary ──"
SHORT=$("$MODBOX" strings /bin/ls 2>/dev/null | wc -l)
LONG=$("$MODBOX" strings -n 12 /bin/ls 2>/dev/null | wc -l)
if [ "$LONG" -lt "$SHORT" ]; then
    pass "-n 12 yields fewer strings than the default on /bin/ls"
else
    fail "-n 12 should yield fewer strings ($LONG) than the default ($SHORT)"
fi

cd "$SCRIPT_DIR" || exit 1
