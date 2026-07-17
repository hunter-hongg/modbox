SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tee ─────────────────────────────────────"

echo "  ── basic tee to single file ──"
printf 'hello\nworld\n' > "$TMPDIR"/input.txt
"$MODBOX" tee "$TMPDIR"/output.txt < "$TMPDIR"/input.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if diff "$TMPDIR"/input.txt "$TMPDIR"/output.txt >/dev/null 2>&1 && diff "$TMPDIR"/input.txt "$TMPDIR"/stdout.txt >/dev/null 2>&1; then
    pass "tee basic"
else
    fail "tee basic — output doesn't match input"
fi

echo "  ── tee to multiple files ──"
"$MODBOX" tee "$TMPDIR"/out1.txt "$TMPDIR"/out2.txt < "$TMPDIR"/input.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if diff "$TMPDIR"/input.txt "$TMPDIR"/out1.txt >/dev/null 2>&1 && diff "$TMPDIR"/input.txt "$TMPDIR"/out2.txt >/dev/null 2>&1 && diff "$TMPDIR"/input.txt "$TMPDIR"/stdout.txt >/dev/null 2>&1; then
    pass "tee multiple files"
else
    fail "tee multiple files — outputs don't match input"
fi

echo "  ── -a append mode ──"
printf 'first line\n' > "$TMPDIR"/append.txt
printf 'second line\n' | "$MODBOX" tee -a "$TMPDIR"/append.txt > "$TMPDIR"/stdout.txt 2>/dev/null
expected=$(printf 'first line\nsecond line\n')
if [[ "$(cat "$TMPDIR"/append.txt)" == "$expected" ]]; then
    pass "tee -a append"
else
    fail "tee -a append — expected [$expected] got [$(cat "$TMPDIR"/append.txt)]"
fi

echo "  ── -a doesn't affect stdout ──"
printf 'new line\n' | "$MODBOX" tee -a "$TMPDIR"/append.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/stdout.txt)" == "$(printf 'new line\n')" ]]; then
    pass "tee -a stdout unaffected"
else
    fail "tee -a stdout unaffected — stdout changed unexpectedly"
fi

echo "  ── overwrite without -a ──"
printf 'original\n' > "$TMPDIR"/overwrite.txt
printf 'new\n' | "$MODBOX" tee "$TMPDIR"/overwrite.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/overwrite.txt)" == "$(printf 'new\n')" ]]; then
    pass "tee overwrite (no -a)"
else
    fail "tee overwrite — file not overwritten"
fi

echo "  ── tee with no files (stdout only) ──"
printf 'stdout test\n' | "$MODBOX" tee > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/stdout.txt)" == "$(printf 'stdout test\n')" ]]; then
    pass "tee no files (stdout only)"
else
    fail "tee no files — stdout doesn't match"
fi

echo "  ── -i ignore-interrupts (accepted option) ──"
printf 'test\n' | "$MODBOX" tee -i "$TMPDIR"/interrupt.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/interrupt.txt)" == "$(printf 'test\n')" ]]; then
    pass "tee -i (option accepted)"
else
    fail "tee -i — output doesn't match"
fi

echo "  ── -p error-action=warn (accepted option) ──"
printf 'test\n' | "$MODBOX" tee -p warn "$TMPDIR"/warn.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/warn.txt)" == "$(printf 'test\n')" ]]; then
    pass "tee -p warn (option accepted)"
else
    fail "tee -p warn — output doesn't match"
fi

echo "  ── -p error-action=warn-nopipe (accepted option) ──"
printf 'test\n' | "$MODBOX" tee -p warn-nopipe "$TMPDIR"/warnnopipe.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/warnnopipe.txt)" == "$(printf 'test\n')" ]]; then
    pass "tee -p warn-nopipe (option accepted)"
else
    fail "tee -p warn-nopipe — output doesn't match"
fi

echo "  ── -p error-action=ignore (accepted option) ──"
printf 'test\n' | "$MODBOX" tee -p ignore "$TMPDIR"/ignore.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if [[ "$(cat "$TMPDIR"/ignore.txt)" == "$(printf 'test\n')" ]]; then
    pass "tee -p ignore (option accepted)"
else
    fail "tee -p ignore — output doesn't match"
fi

echo "  ── error: invalid error-action mode ──"
if printf 'test\n' | "$MODBOX" tee -p invalid "$TMPDIR"/invalid.txt 2>&1 | grep -q "invalid error action"; then
    pass "tee invalid error-action (error reported)"
else
    fail "tee invalid error-action — error not reported"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' tee --help
assert_cmd_pat 'append' tee --help
assert_cmd_pat 'ignore-interrupts' tee --help
assert_cmd_pat 'error-action' tee --help

echo "  ── -h ──"
assert_cmd_pat 'Usage:' tee -h

echo "  ── binary data handling ──"
printf '\x00\x01\x02\x03' | "$MODBOX" tee "$TMPDIR"/binary.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if diff "$TMPDIR"/binary.txt "$TMPDIR"/stdout.txt >/dev/null 2>&1; then
    pass "tee binary data"
else
    fail "tee binary data — outputs don't match"
fi

echo "  ── long input ──"
# Generate a longer input to test buffering
for i in {1..100}; do echo "Line $i"; done > "$TMPDIR"/long.txt
"$MODBOX" tee "$TMPDIR"/longout.txt < "$TMPDIR"/long.txt > "$TMPDIR"/stdout.txt 2>/dev/null
if diff "$TMPDIR"/long.txt "$TMPDIR"/longout.txt >/dev/null 2>&1 && diff "$TMPDIR"/long.txt "$TMPDIR"/stdout.txt >/dev/null 2>&1; then
    pass "tee long input"
else
    fail "tee long input — outputs don't match"
fi
