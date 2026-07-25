SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── truncate ───────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/trunc_basic.txt
printf 'abcdefghij' > "$TMPDIR"/trunc_10.txt
: > "$TMPDIR"/trunc_empty.txt

echo "  ── -s 5 : shrink to 5 bytes ──"
"$MODBOX" truncate -s 5 "$TMPDIR"/trunc_basic.txt
result=$(cat "$TMPDIR"/trunc_basic.txt)
if [[ "$result" == "hello" ]]; then
    pass "truncate -s 5 shrinks to 'hello'"
else
    fail "truncate -s 5 — expected 'hello' got [$result]"
fi

echo "  ── -s 20 : extend with sparse zeros ──"
"$MODBOX" truncate -s 20 "$TMPDIR"/trunc_basic.txt
size=$(stat -c%s "$TMPDIR"/trunc_basic.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_basic.txt)
if [[ "$size" -eq 20 ]]; then
    pass "truncate -s 20 extends file to 20 bytes"
else
    fail "truncate -s 20 — expected size 20, got $size"
fi

echo "  ── -s 0 : shrink to empty ──"
"$MODBOX" truncate -s 0 "$TMPDIR"/trunc_10.txt
size=$(stat -c%s "$TMPDIR"/trunc_10.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_10.txt)
if [[ "$size" -eq 0 ]]; then
    pass "truncate -s 0 shrinks to empty"
else
    fail "truncate -s 0 — expected size 0, got $size"
fi

echo "  ── -s +3 : relative increase ──"
printf 'abc' > "$TMPDIR"/trunc_rel.txt
"$MODBOX" truncate -s +3 "$TMPDIR"/trunc_rel.txt
size=$(stat -c%s "$TMPDIR"/trunc_rel.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_rel.txt)
if [[ "$size" -eq 6 ]]; then
    pass "truncate -s +3 increases size by 3"
else
    fail "truncate -s +3 — expected size 6, got $size"
fi

echo "  ── -s -2 : relative decrease ──"
printf 'abcdef' > "$TMPDIR"/trunc_rel2.txt
"$MODBOX" truncate -s -2 "$TMPDIR"/trunc_rel2.txt
size=$(stat -c%s "$TMPDIR"/trunc_rel2.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_rel2.txt)
if [[ "$size" -eq 4 ]]; then
    pass "truncate -s -2 decreases size by 2"
else
    fail "truncate -s -2 — expected size 4, got $size"
fi

echo "  ── -r REFERENCE : use reference file size ──"
printf '12345' > "$TMPDIR"/trunc_ref.txt
printf 'hello' > "$TMPDIR"/trunc_target.txt
"$MODBOX" truncate -r "$TMPDIR"/trunc_target.txt "$TMPDIR"/trunc_ref.txt
size=$(stat -c%s "$TMPDIR"/trunc_ref.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_ref.txt)
if [[ "$size" -eq 5 ]]; then
    pass "truncate -r sets size to reference file"
else
    fail "truncate -r — expected size 5, got $size"
fi

echo "  ── -c --no-create : do not create missing file ──"
"$MODBOX" truncate -c "$TMPDIR"/trunc_nonexistent.txt 2>/dev/null
if [[ ! -f "$TMPDIR"/trunc_nonexistent.txt ]]; then
    pass "truncate -c does not create missing file"
else
    fail "truncate -c created file that shouldn't exist"
fi

echo "  ── create new file when not using -c ──"
"$MODBOX" truncate -s 10 "$TMPDIR"/trunc_new.txt 2>/dev/null
if [[ -f "$TMPDIR"/trunc_new.txt ]]; then
    size=$(stat -c%s "$TMPDIR"/trunc_new.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_new.txt)
    if [[ "$size" -eq 10 ]]; then
        pass "truncate creates new file with specified size"
    else
        fail "truncate created file but size is $size, expected 10"
    fi
else
    fail "truncate did not create new file"
fi

echo "  ── size with K suffix ──"
"$MODBOX" truncate -s 1K "$TMPDIR"/trunc_k.txt 2>/dev/null
size=$(stat -c%s "$TMPDIR"/trunc_k.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_k.txt)
if [[ "$size" -eq 1024 ]]; then
    pass "truncate -s 1K creates 1024 byte file"
else
    fail "truncate -s 1K — expected size 1024, got $size"
fi

echo "  ── multiple files ──"
printf 'abc' > "$TMPDIR"/trunc_m1.txt
printf 'def' > "$TMPDIR"/trunc_m2.txt
"$MODBOX" truncate -s 1 "$TMPDIR"/trunc_m1.txt "$TMPDIR"/trunc_m2.txt
size1=$(stat -c%s "$TMPDIR"/trunc_m1.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_m1.txt)
size2=$(stat -c%s "$TMPDIR"/trunc_m2.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_m2.txt)
if [[ "$size1" -eq 1 && "$size2" -eq 1 ]]; then
    pass "truncate operates on multiple files"
else
    fail "truncate multiple files — expected both size 1, got $size1 and $size2"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' truncate --help

echo "  ── nonexistent file is created without -c ──"
"$MODBOX" truncate -s 10 "$TMPDIR"/trunc_noexist.txt 2>/dev/null
if [[ -f "$TMPDIR"/trunc_noexist.txt ]]; then
    size=$(stat -c%s "$TMPDIR"/trunc_noexist.txt 2>/dev/null || stat -f%z "$TMPDIR"/trunc_noexist.txt)
    if [[ "$size" -eq 10 ]]; then
        pass "truncate creates nonexistent file with specified size"
    else
        fail "truncate created file but size is $size, expected 10"
    fi
else
    fail "truncate did not create nonexistent file"
fi
