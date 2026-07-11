#!/usr/bin/env bash
#
# test_awk.sh — Tests for the modbox awk command (GNU awk compatible subset)

echo "── awk ───────────────────────────────────────────────────────────────"

# Basic field printing
assert_cmd "a c
d f" awk '{print $1, $3}' <(printf 'a b c\nd e f\n')

# NF / NR
assert_cmd "1 3
2 2" awk '{print NR, NF}' <(printf '1 2 3\n4 5\n')

# Regex pattern (prints matching records)
assert_cmd "bar
baz" awk '/ba/' <(printf 'foo\nbar\nbaz\n')

# Numeric condition
assert_cmd "2
4" awk '$1 % 2 == 0' <(printf '1\n2\n3\n4\n')

# BEGIN / END
assert_cmd "S
x
y
E" awk 'BEGIN{print "S"} {print} END{print "E"}' <(printf 'x\ny\n')

# Summation in END
assert_cmd "6" awk '{s+=$1} END{print s}' <(printf '1\n2\n3\n')

# Field separator
assert_cmd "b" awk -F: '{print $2}' <(printf 'a:b:c\n')

# printf formatting
assert_cmd "3.0
5.0" awk '{printf "%.1f\n", $1*2}' <(printf '1.5\n2.5\n')

# Range pattern
assert_cmd "b
c
d" awk '/b/,/d/' <(printf 'a\nb\nc\nd\ne\n')

# print with no args prints $0
assert_cmd "x
y" awk '{print}' <(printf 'x\ny\n')

# String concatenation
assert_cmd "abXab" awk '{print $1 "X" $1}' <(printf 'ab\n')

# if / for
assert_cmd "big
15" awk '{if($1>3) print "big"; for(i=1;i<=$1;i++) s+=i; print s}' <(printf '5\n')

# match operator
assert_cmd "yes" awk '$0 ~ /world/ {print "yes"}' <(printf 'hello world\n')

# function definition and call
assert_cmd "9" awk 'function sq(n){return n*n} {print sq($1)}' <(printf '3\n')

# associative array with for-in (order is unspecified; check each entry)
"$MODBOX" awk '{for(i=1;i<=NF;i++) cnt[$i]++} END{for(k in cnt) print k, cnt[k]}' <(printf 'a b c\n') > "$TMPDIR/awk_arr.out" 2>/dev/null
assert_cmd_pat "a 1" cat "$TMPDIR/awk_arr.out"
assert_cmd_pat "b 1" cat "$TMPDIR/awk_arr.out"
assert_cmd_pat "c 1" cat "$TMPDIR/awk_arr.out"

# while loop
assert_cmd "15" awk '{i=1; while(i<=$1){s+=i;i++} print s}' <(printf '5\n')

# -v assignment
assert_cmd "10" awk -v x=10 'BEGIN{print x}'

# gsub
assert_cmd "a-b-c" awk '{gsub(/,/,"-"); print}' <(printf 'a,b,c\n')

# OFS
assert_cmd "a|b" awk 'BEGIN{OFS="|"} {print $1,$2}' <(printf 'a b\n')

# next
assert_cmd "$(printf '1\n3')" awk '/2/ {next} {print}' <(printf '1\n2\n3\n')

# -f program file
printf '{print $1}\n' > "$TMPDIR/prog.awk"
assert_cmd "p" awk -f "$TMPDIR/prog.awk" <(printf 'p q\n')

# stdin via -
assert_cmd "z" awk '{print}' - <(printf 'z\n')

# -- ends options
assert_cmd "x" awk -- '{print}' <(printf 'x\n')

echo "── awk done ──────────────────────────────────────────────────────────"
