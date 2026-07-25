SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── diff3 ──────────────────────────────────────"

# Create test files
printf 'a\nb\nc\n' > "$TMPDIR"/d3_mine
printf 'a\nx\nc\n' > "$TMPDIR"/d3_base
printf 'a\ny\nc\n' > "$TMPDIR"/d3_yours
printf 'a\nb\nc\n' > "$TMPDIR"/d3_identical1
printf 'a\nb\nc\n' > "$TMPDIR"/d3_identical2

echo "  ── default output with conflict ──"
output=$("$MODBOX" diff3 "$TMPDIR"/d3_mine "$TMPDIR"/d3_base "$TMPDIR"/d3_yours 2>/dev/null)
if echo "$output" | grep -qE '^2,2c2,2' && echo "$output" | grep -qE '^> b$' && echo "$output" | grep -qE '^---$' && echo "$output" | grep -qE '^< y$'; then
    pass "diff3 default output shows conflict"
else
    fail "diff3 default output — unexpected format: [$output]"
fi

echo "  ── identical files produce no output ──"
output=$("$MODBOX" diff3 "$TMPDIR"/d3_identical1 "$TMPDIR"/d3_identical2 "$TMPDIR"/d3_identical1 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "diff3 identical files produce no output"
else
    fail "diff3 identical files — expected empty output, got [$output]"
fi

echo "  ── -m merge output with conflict markers ──"
output=$("$MODBOX" diff3 -m "$TMPDIR"/d3_mine "$TMPDIR"/d3_base "$TMPDIR"/d3_yours 2>/dev/null)
if echo "$output" | grep -qE '^<<<<<<<' && echo "$output" | grep -qE '^=======$' && echo "$output" | grep -qE '^>>>>>>>'; then
    pass "diff3 -m outputs conflict markers"
else
    fail "diff3 -m — missing conflict markers: [$output]"
fi

echo "  ── -m merge: unchanged lines preserved ──"
if echo "$output" | grep -qE '^a$' && echo "$output" | grep -qE '^c$'; then
    pass "diff3 -m preserves unchanged lines"
else
    fail "diff3 -m — missing unchanged lines: [$output]"
fi

echo "  ── -e ed script format ──"
output=$("$MODBOX" diff3 -e "$TMPDIR"/d3_mine "$TMPDIR"/d3_base "$TMPDIR"/d3_yours 2>/dev/null)
if echo "$output" | grep -qE '^[0-9]+a$' && echo "$output" | grep -qE '^\.$' && echo "$output" | grep -qE '^w$' && echo "$output" | grep -qE '^q$'; then
    pass "diff3 -e outputs ed script"
else
    fail "diff3 -e — unexpected ed format: [$output]"
fi

echo "  ── stdin via - ──"
printf 'a\nz\nc\n' | "$MODBOX" diff3 - "$TMPDIR"/d3_base "$TMPDIR"/d3_yours 2>/dev/null | grep -q 'z' && pass "diff3 reads stdin via -" || fail "diff3 stdin via -"

echo "  ── non-existent file ──"
assert_cmd_pat_stderr 'No such file' diff3 "$TMPDIR"/d3_nonexistent "$TMPDIR"/d3_base "$TMPDIR"/d3_yours

echo "  ── help ──"
assert_cmd_pat 'Usage:' diff3 --help

echo "  ── -L labels ──"
output=$("$MODBOX" diff3 -m -L mine -L base -L yours "$TMPDIR"/d3_mine "$TMPDIR"/d3_base "$TMPDIR"/d3_yours 2>/dev/null)
if echo "$output" | grep -qE '^<<<<<<< mine$' && echo "$output" | grep -qE '^>>>>>>> yours$'; then
    pass "diff3 -L uses custom labels"
else
    fail "diff3 -L — custom labels not found: [$output]"
fi
