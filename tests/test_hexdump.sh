#!/usr/bin/env bash
# hexdump - display file contents in hexadecimal, octal, decimal and ASCII
#
# Every expected value is taken from the host hexdump, which is the behaviour
# this command reproduces. Each case compares the whole output byte for byte
# rather than pattern matching, because the column alignment is what a format
# change breaks silently.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "-- hexdump --------------------------------------------------"

echo "  -- --help / --version --"
assert_cmd_pat 'Usage:' hexdump --help
assert_cmd_pat 'hexdump' hexdump --help
assert_cmd_pat 'hexdump \(modbox\)' hexdump --version

# assert_stdin DESCRIPTION EXPECTED FIXTURE [args...]
# Feeds a fixture to hexdump on standard input and compares the entire output
# with EXPECTED. Writing the payload to a file is what lets a NUL byte reach
# the command, which the escape cases depend on.
assert_stdin() {
    local desc="$1" expected="$2" fixture="$3"; shift 3
    local got
    got=$("$MODBOX" hexdump "$@" 2>&1 < "$HEXDIR/$fixture.bin")
    if [[ "$got" == "$expected" ]]; then
        pass "$desc"
    else
        fail "$desc"
        echo "         expected: $(printf '%q' "$expected")" >&2
        echo "         got:      $(printf '%q' "$got")" >&2
    fi
}

# assert_files DESCRIPTION EXPECTED args...
# Runs hexdump over files named on the command line and compares the output.
assert_files() {
    local desc="$1" expected="$2"; shift 2
    local got
    got=$("$MODBOX" hexdump "$@" 2>&1 </dev/null)
    if [[ "$got" == "$expected" ]]; then
        pass "$desc"
    else
        fail "$desc"
        echo "         expected: $(printf '%q' "$expected")" >&2
        echo "         got:      $(printf '%q' "$got")" >&2
    fi
}

# The reference vector used below is "ABCDEFGHIJKLMNOP". The default format
# groups the bytes in pairs and prints them in the host's native order, which
# is little endian on every platform modbox targets.
HEXDIR="$TMPDIR/hexdump_fixtures"
mkdir -p "$HEXDIR"

printf '%b' 'ABCDEFGHIJKLMNOP' > "$HEXDIR/abc16.bin"
printf '%b' 'ABCDEFGH' > "$HEXDIR/short.bin"
printf '%b' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ012345' > "$HEXDIR/abc32.bin"
printf '%b' 'AAAA' > "$HEXDIR/aaaa.bin"
printf '%b' 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' > "$HEXDIR/aaaa64.bin"
printf '%b' '\000\001\002' > "$HEXDIR/dots.bin"
printf '%b' 'hello\000\001\002world' > "$HEXDIR/hello.bin"
printf '%b' '\336\012\302U,\366H\340\002\263?\022\0119\363"' > "$HEXDIR/ctrl.bin"
printf '%b' 'ABCD'     > "$HEXDIR/f4.bin"
printf '%b' 'ABCDEFGH' > "$HEXDIR/f8.bin"
printf 'AAAAAAAA%.0s' 1 2 3 4 5 6 7 8 > "$HEXDIR/squeeze.bin"
printf '%b' '\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300\301\302\303\304\305\306\307' > "$HEXDIR/r200.bin"

echo "  -- default format (two byte hexadecimal) --"
assert_stdin "default layout" \
    $'0000000 4241 4443 4645 4847 4a49 4c4b 4e4d 504f\n0000010' \
    "abc16"
assert_stdin "-x two byte hex" \
    $'0000000    4241    4443    4645    4847    4a49    4c4b    4e4d    504f\n0000010' \
    "abc16" -x
assert_stdin "-X one byte hex" \
    $'0000000  41  42  43  44  45  46  47  48  49  4a  4b  4c  4d  4e  4f  50\n0000010' \
    "abc16" -X
assert_stdin "-b one byte octal" \
    $'0000000 101 102 103 104 105 106 107 110 111 112 113 114 115 116 117 120\n0000010' \
    "abc16" -b
assert_stdin "-o two byte octal" \
    $'0000000  041101  042103  043105  044107  045111  046113  047115  050117\n0000010' \
    "abc16" -o
assert_stdin "-d two byte decimal" \
    $'0000000   16961   17475   17989   18503   19017   19531   20045   20559\n0000010' \
    "abc16" -d

echo "  -- canonical format --"
assert_stdin "-C hex plus ASCII gutter" \
    $'00000000  41 42 43 44 45 46 47 48  49 4a 4b 4c 4d 4e 4f 50  |ABCDEFGHIJKLMNOP|\n00000010' \
    "abc16" -C
assert_stdin "-C trims the gutter on a short line" \
    $'00000000  41 42 43 44 45 46 47 48                           |ABCDEFGH|\n00000008' \
    "short" -C
assert_stdin "-C renders unprintable bytes as dots" \
    $'00000000  00 01 02                                          |...|\n00000003' \
    "dots" -C

echo "  -- one character format --"
assert_stdin "-c right aligns each byte" \
    $'0000000   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O   P\n0000010' \
    "abc16" -c
assert_stdin "-c escapes a zero byte" \
    $'0000000   h   e   l   l   o  \\0 001 002   w   o   r   l   d            \n000000d' \
    "hello" -c
assert_stdin "-c renders other control bytes as octal" \
    $'0000000 336  \\n 302   U   , 366   H 340 002 263   ? 022  \\t   9 363   "\n0000010' \
    "ctrl" -c

echo "  -- format options interleave per data block --"
# Every data block is printed once per requested format before the next block
# begins, which is not the same as running one whole pass per format.
assert_files "-C -x interleaves the two formats" \
    $'00000000  41 42 43 44 45 46 47 48  49 4a 4b 4c 4d 4e 4f 50  |ABCDEFGHIJKLMNOP|\n0000000    4241    4443    4645    4847    4a49    4c4b    4e4d    504f\n00000010  51 52 53 54 55 56 57 58  59 5a 30 31 32 33 34 35  |QRSTUVWXYZ012345|\n0000010    5251    5453    5655    5857    5a59    3130    3332    3534\n0000020' \
    -C -x "$HEXDIR/abc32.bin"

# Naming a format twice makes it print every block rather than being squeezed
# against its own first occurrence.
assert_files "a repeated format still prints every block" \
    $'0000000 101 101 101 101                                                \n0000000 101 101 101 101                                                \n0000004' \
    -b -b "$HEXDIR/aaaa.bin"

# A run of identical blocks is replaced by a single star even with several
# formats, where the whole block repeats.
assert_files "a repeated block yields one star with two formats" \
    $'00000000  41 41 41 41 41 41 41 41  41 41 41 41 41 41 41 41  |AAAAAAAAAAAAAAAA|\n0000000    4141    4141    4141    4141    4141    4141    4141    4141\n*\n0000030' \
    -C -x "$HEXDIR/aaaa64.bin"

echo "  -- format options combine, in order --"
assert_stdin "-C -x runs both formats" \
    $'00000000  41 42 43 44 45 46 47 48  49 4a 4b 4c 4d 4e 4f 50  |ABCDEFGHIJKLMNOP|\n0000000    4241    4443    4645    4847    4a49    4c4b    4e4d    504f\n0000010' \
    "abc16" -C -x
assert_stdin "-x -C runs both formats in order" \
    $'0000000    4241    4443    4645    4847    4a49    4c4b    4e4d    504f\n00000000  41 42 43 44 45 46 47 48  49 4a 4b 4c 4d 4e 4f 50  |ABCDEFGHIJKLMNOP|\n00000010' \
    "abc16" -x -C

echo "  -- -n limits the byte count --"
assert_stdin "-n 8 stops after eight bytes" \
    $'0000000 4241 4443 4645 4847                    \n0000008' \
    "abc16" -n 8
assert_stdin "-n 0 prints nothing" \
    '' \
    "abc16" -n 0
assert_stdin "-n 5 zero pads the last unit" \
    $'0000000 4241 4443 0045                         \n0000005' \
    "abc16" -n 5

echo "  -- files form one continuous stream --"
assert_files "two files continue the offsets" \
    $'0000000 4241 4443 4241 4443 4645 4847          \n000000c' \
    "$HEXDIR/f4.bin" "$HEXDIR/f8.bin"
assert_files "-n spans the files" \
    $'0000000 4241 4443 4645 4847 4241 4443          \n000000c' \
    -n 12 "$HEXDIR/f8.bin" "$HEXDIR/f8.bin" "$HEXDIR/f8.bin"

echo "  -- -s skips into the stream --"
assert_files "-s 4 drops the first four bytes" \
    $'0000004 4645 4847 4a49 4c4b 4e4d 504f          \n0000010' \
    -s 4 "$HEXDIR/abc16.bin"
assert_files "-s accepts a hexadecimal count" \
    $'0000004 4645 4847 4a49 4c4b 4e4d 504f          \n0000010' \
    -s 0x4 "$HEXDIR/abc16.bin"
assert_files "-s past the end reports the length" \
    $'00000c8' \
    -s 300 "$HEXDIR/r200.bin"

echo "  -- -v disables the duplicate squeeze --"
assert_files "repeated lines are squeezed to a star" \
    $'0000000 4141 4141 4141 4141 4141 4141 4141 4141\n*\n0000040' \
    "$HEXDIR/squeeze.bin"
assert_files "-v keeps every repeated line" \
    $'0000000 4141 4141 4141 4141 4141 4141 4141 4141\n0000010 4141 4141 4141 4141 4141 4141 4141 4141\n0000020 4141 4141 4141 4141 4141 4141 4141 4141\n0000030 4141 4141 4141 4141 4141 4141 4141 4141\n0000040' \
    -v "$HEXDIR/squeeze.bin"

echo "  -- errors --"
assert_cmd_pat_stderr 'No such file' hexdump /nonexistent/file
assert_cmd_pat_stderr 'invalid skip' hexdump -s bogus /dev/null

echo ""
echo "Results: $PASS_COUNT passed, $FAIL_COUNT failed"
[[ $FAIL_COUNT -eq 0 ]]
