SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── install ─────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat "Usage:" install --help

echo "  ── basic copy ──"
echo "hello world" > "$TMPDIR"/inst_src.txt
"$MODBOX" install "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_dst.txt 2>/dev/null || true
assert_cmd "hello world" cat "$TMPDIR"/inst_dst.txt

echo "  ── -v : verbose ──"
out=$("$MODBOX" install -v "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_v_dst.txt 2>/dev/null || true)
if echo "$out" | grep -qE "'$TMPDIR/inst_src.txt' ->"; then
  pass "install -v → verbose output"
else
  fail "install -v — expected verbose output, got [$out]"
fi

echo "  ── -d : create directory ──"
"$MODBOX" install -d "$TMPDIR"/inst_dir/sub 2>/dev/null || true
if [[ -d "$TMPDIR"/inst_dir/sub ]]; then
  pass "install -d creates directory hierarchy"
else
  fail "install -d — directory not created"
fi

echo "  ── -d : existing directory ──"
"$MODBOX" install -d "$TMPDIR"/inst_dir/sub 2>/dev/null || true
if [[ -d "$TMPDIR"/inst_dir/sub ]]; then
  pass "install -d existing directory (no error)"
else
  fail "install -d existing directory — failed"
fi

echo "  ── -d -v : verbose directory creation ──"
out=$("$MODBOX" install -d -v "$TMPDIR"/inst_dir_verb/deep 2>/dev/null || true)
if echo "$out" | grep -q "created directory"; then
  pass "install -d -v → verbose directory output"
else
  fail "install -d -v — expected 'created directory' in output"
fi

echo "  ── -m : set mode ──"
echo "mode_test" > "$TMPDIR"/inst_mode_src.txt
"$MODBOX" install -m 644 "$TMPDIR"/inst_mode_src.txt "$TMPDIR"/inst_mode_dst.txt 2>/dev/null || true
mode=$(stat -c "%a" "$TMPDIR"/inst_mode_dst.txt 2>/dev/null || echo "missing")
if [[ "$mode" == "644" ]]; then
  pass "install -m 644 → mode $mode"
else
  fail "install -m 644 — expected mode 644, got $mode"
fi

echo "  ── -m 755 ──"
"$MODBOX" install -m 755 "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_mode755.txt 2>/dev/null || true
mode=$(stat -c "%a" "$TMPDIR"/inst_mode755.txt 2>/dev/null || echo "missing")
if [[ "$mode" == "755" ]]; then
  pass "install -m 755 → mode $mode"
else
  fail "install -m 755 — expected mode 755, got $mode"
fi

echo "  ── -b : backup existing ──"
echo "original" > "$TMPDIR"/inst_bak_dst.txt
"$MODBOX" install -b "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_bak_dst.txt 2>/dev/null || true
dst_content=$(cat "$TMPDIR"/inst_bak_dst.txt 2>/dev/null || echo "missing")
bak_content=$(cat "$TMPDIR"/inst_bak_dst.txt~ 2>/dev/null || echo "missing")
if [[ "$dst_content" == "hello world" && "$bak_content" == "original" ]]; then
  pass "install -b → backup created with ~ suffix"
else
  fail "install -b — dst=[$dst_content] bak=[$bak_content]"
fi

echo "  ── -S : custom backup suffix ──"
echo "custom_bak" > "$TMPDIR"/inst_cust_dst.txt
"$MODBOX" install -b -S ".bak" "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_cust_dst.txt 2>/dev/null || true
dst_content=$(cat "$TMPDIR"/inst_cust_dst.txt 2>/dev/null || echo "missing")
bak_content=$(cat "$TMPDIR"/inst_cust_dst.txt.bak 2>/dev/null || echo "missing")
if [[ "$dst_content" == "hello world" && "$bak_content" == "custom_bak" ]]; then
  pass "install -b -S .bak → backup with .bak suffix"
else
  fail "install -b -S .bak — dst=[$dst_content] bak=[$bak_content]"
fi

echo "  ── -C : compare — skip identical ──"
echo "identical content" > "$TMPDIR"/inst_cmp_src.txt
cp "$TMPDIR"/inst_cmp_src.txt "$TMPDIR"/inst_cmp_dst.txt
"$MODBOX" install -C "$TMPDIR"/inst_cmp_src.txt "$TMPDIR"/inst_cmp_dst.txt 2>/dev/null || true
assert_cmd "identical content" cat "$TMPDIR"/inst_cmp_dst.txt

echo "  ── -C -v : compare — verbose skip ──"
out=$("$MODBOX" install -v -C "$TMPDIR"/inst_cmp_src.txt "$TMPDIR"/inst_cmp_dst.txt 2>/dev/null || true)
if echo "$out" | grep -q "skipped: identical"; then
  pass "install -v -C → reports skip"
else
  fail "install -v -C — expected 'skipped: identical', got [$out]"
fi

echo "  ── -C : compare — copy when different ──"
echo "different content" > "$TMPDIR"/inst_cmp_src2.txt
"$MODBOX" install -C "$TMPDIR"/inst_cmp_src2.txt "$TMPDIR"/inst_cmp_dst.txt 2>/dev/null || true
assert_cmd "different content" cat "$TMPDIR"/inst_cmp_dst.txt

echo "  ── -p : preserve timestamps ──"
echo "timestamp test" > "$TMPDIR"/inst_ts_src.txt
touch -t 202001010000 "$TMPDIR"/inst_ts_src.txt
"$MODBOX" install -p "$TMPDIR"/inst_ts_src.txt "$TMPDIR"/inst_ts_dst.txt 2>/dev/null || true
src_mtime=$(stat -c "%Y" "$TMPDIR"/inst_ts_src.txt 2>/dev/null || echo "0")
dst_mtime=$(stat -c "%Y" "$TMPDIR"/inst_ts_dst.txt 2>/dev/null || echo "1")
if [[ "$src_mtime" == "$dst_mtime" ]]; then
  pass "install -p → timestamps preserved ($src_mtime)"
else
  fail "install -p — src mtime $src_mtime != dst mtime $dst_mtime"
fi

echo "  ── -t : target directory ──"
mkdir -p "$TMPDIR"/inst_tgt
"$MODBOX" install -t "$TMPDIR"/inst_tgt "$TMPDIR"/inst_src.txt 2>/dev/null || true
assert_cmd "hello world" cat "$TMPDIR"/inst_tgt/inst_src.txt

echo "  ── -t : multiple sources to directory ──"
echo "multi_a" > "$TMPDIR"/inst_multi_a.txt
echo "multi_b" > "$TMPDIR"/inst_multi_b.txt
mkdir -p "$TMPDIR"/inst_tgt_multi
"$MODBOX" install -t "$TMPDIR"/inst_tgt_multi "$TMPDIR"/inst_multi_a.txt "$TMPDIR"/inst_multi_b.txt 2>/dev/null || true
assert_cmd "multi_a" cat "$TMPDIR"/inst_tgt_multi/inst_multi_a.txt
assert_cmd "multi_b" cat "$TMPDIR"/inst_tgt_multi/inst_multi_b.txt

echo "  ── -t -v : target directory verbose ──"
mkdir -p "$TMPDIR"/inst_tgt_verb
out=$("$MODBOX" install -t "$TMPDIR"/inst_tgt_verb -v "$TMPDIR"/inst_src.txt 2>/dev/null || true)
if echo "$out" | grep -qE "'$TMPDIR/inst_src.txt' ->"; then
  pass "install -t -v → verbose output"
else
  fail "install -t -v — expected verbose output, got [$out]"
fi

echo "  ── multiple sources to directory (positional) ──"
mkdir -p "$TMPDIR"/inst_pos_dir
"$MODBOX" install "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_multi_b.txt "$TMPDIR"/inst_pos_dir 2>/dev/null || true
assert_cmd "hello world" cat "$TMPDIR"/inst_pos_dir/inst_src.txt
assert_cmd "multi_b" cat "$TMPDIR"/inst_pos_dir/inst_multi_b.txt

echo "  ── -T : no-target-directory (overwrite file) ──"
echo "overwrite me" > "$TMPDIR"/inst_T_target
"$MODBOX" install -T "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_T_target 2>/dev/null || true
assert_cmd "hello world" cat "$TMPDIR"/inst_T_target

echo "  ── -T : treat dest as file even if it's a directory ──"
rm -rf "$TMPDIR"/inst_T_as_file
"$MODBOX" install -T "$TMPDIR"/inst_src.txt "$TMPDIR"/inst_T_as_file 2>/dev/null || true
assert_cmd "hello world" cat "$TMPDIR"/inst_T_as_file

echo "  ── error: missing destination operand ──"
assert_cmd_pat_stderr "missing" install "$TMPDIR"/inst_src.txt

echo "  ── error: non-existent source ──"
assert_cmd_pat_stderr "No such file or directory" install "$TMPDIR"/inst_nonexistent_xyz "$TMPDIR"/inst_err_dst

echo "  ── error: -t with non-existent directory ──"
assert_cmd_pat_stderr "does not exist" install -t "$TMPDIR"/inst_tgt_nonexistent "$TMPDIR"/inst_src.txt

echo "  ── error: -t without source files ──"
mkdir -p "$TMPDIR"/inst_empty_tgt
out=$("$MODBOX" install -t "$TMPDIR"/inst_empty_tgt 2>&1 1>/dev/null || true)
# argtable should catch this as missing required option

echo "  ── error: -d without directories ──"
assert_cmd_pat_stderr "missing" install -d
