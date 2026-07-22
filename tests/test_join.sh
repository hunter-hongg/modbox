SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── join ──────────────────────────────────────"

printf '01\tAlice\n02\tBob\n03\tCharlie\n' > "$TMPDIR"/join_a
printf '01\t5\n02\t8\n03\t1\n04\t9\n' > "$TMPDIR"/join_b

echo " ── basic join on default field 1 ──"
result=$("$MODBOX" join "$TMPDIR"/join_a "$TMPDIR"/join_b 2>/dev/null || true)
if echo "$result" | grep -q '01'; then
  pass "basic join — found key 01 in output"
else
  fail "basic join — expected key 01 in output, got [$result]"
fi

if echo "$result" | grep -q '02\tBob\t8'; then
  pass "basic join — 02 Bob matched 8"
else
  fail "basic join — expected '02\tBob\t8' in output"
fi

echo " ── custom delimiter ──"
printf 'a|x\nb|y\nc|z\n' > "$TMPDIR"/join_c
printf 'x|1\ny|2\nz|3\n' > "$TMPDIR"/join_d
result=$("$MODBOX" join -t'|' "$TMPDIR"/join_c "$TMPDIR"/join_d 2>/dev/null || true)
if echo "$result" | grep -q 'a|x|1'; then
  pass "custom delimiter — joined with |"
else
  fail "custom delimiter — expected 'a|x|1', got [$result]"
fi

echo " ── --help shows usage ──"
assert_cmd_pat 'Usage:' join --help
echo " ── --version ──"
assert_cmd_pat 'join (modbox)' join --version
