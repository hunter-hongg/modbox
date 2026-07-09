echo ""
echo "── tsort ──────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' tsort --help

echo "  ── basic linear chain ──"
printf 'a b\nb c\nc d\n' | assert_cmd "$(printf 'a\nb\nc\nd\n')" tsort

echo "  ── multiple independent chains ──"
printf 'a b\nc d\n' | assert_cmd "$(printf 'a\nc\nb\nd\n')" tsort

echo "  ── diamond ──"
printf 'a b\na c\nb d\nc d\n' | assert_cmd "$(printf 'a\nb\nc\nd\n')" tsort

echo "  ── cycle detection ──"
result=$(printf 'a b\nb c\nc a\n' | "$MODBOX" tsort 2>&1 || true)
expected=$(printf 'tsort: input contains a cycle\na\nb\nc\n')
if [ "$result" = "$expected" ]; then
    pass "tsort (cycle)"
else
    fail "tsort (cycle) — expected [$(echo "$expected" | head -c 40)] got [$(echo "$result" | head -c 40)]"
fi

echo "  ── cycle exit code 1 ──"
if printf 'a b\nb a\n' | "$MODBOX" tsort >/dev/null 2>&1; then
    fail "tsort (cycle exit) — expected exit 1, got 0"
else
    pass "tsort (cycle exit 1)"
fi

echo "  ── file input ──"
printf 'x y\ny z\n' > "$TMPDIR"/tsort_file
assert_cmd "$(printf 'x\ny\nz\n')" tsort "$TMPDIR"/tsort_file

echo "  ── single item (odd tokens) ──"
printf 'a\n' | assert_cmd "a" tsort

echo "  ── single pair ──"
printf 'first second\n' | assert_cmd "$(printf 'first\nsecond\n')" tsort

echo "  ── stdin (using -) ──"
printf 'p q\n' | assert_cmd "$(printf 'p\nq\n')" tsort -

echo "  ── empty input ──"
printf '' | assert_cmd "" tsort

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr "cannot open" tsort "$TMPDIR"/tsort_nonexistent
