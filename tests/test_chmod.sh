SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── chmod ──────────────────────────────────────────"

echo "  ── setup ──"
echo "readonly file" > "$TMPDIR"/chmod_ro.txt
chmod 444 "$TMPDIR"/chmod_ro.txt
echo "readwrite file" > "$TMPDIR"/chmod_rw.txt
chmod 644 "$TMPDIR"/chmod_rw.txt
echo "executable file" > "$TMPDIR"/chmod_exec.txt
chmod 755 "$TMPDIR"/chmod_exec.txt
mkdir -p "$TMPDIR"/chmod_dir/sub
echo "nested" > "$TMPDIR"/chmod_dir/sub/file.txt
echo "test file" > "$TMPDIR"/chmod_ref.txt
chmod 644 "$TMPDIR"/chmod_ref.txt

echo "  ── --help ──"
assert_cmd_pat 'Usage:' chmod --help

echo "  ── missing operand ──"
assert_cmd_pat_stderr 'missing operand' chmod

echo "  ── invalid mode ──"
assert_cmd_pat_stderr 'invalid mode' chmod badmode "$TMPDIR"/chmod_rw.txt

echo "  ── nonexistent file ──"
assert_cmd_pat_stderr 'No such file' chmod 644 "$TMPDIR"/chmod_nonexistent

echo "  ── octal: 644 ──"
"$MODBOX" chmod 644 "$TMPDIR"/chmod_rw.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_rw.txt)
if [[ "$mode" == "644" ]]; then
    pass "chmod 644 → mode is 644"
else
    fail "chmod 644 — expected mode 644, got $mode"
fi

echo "  ── octal: 755 ──"
"$MODBOX" chmod 755 "$TMPDIR"/chmod_rw.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_rw.txt)
if [[ "$mode" == "755" ]]; then
    pass "chmod 755 → mode is 755"
else
    fail "chmod 755 — expected mode 755, got $mode"
fi

echo "  ── octal: 000 ──"
"$MODBOX" chmod 000 "$TMPDIR"/chmod_rw.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_rw.txt)
if [[ "$mode" == "0" || "$mode" == "000" ]]; then
    pass "chmod 000 → mode is $mode"
else
    fail "chmod 000 — expected mode 0/000, got $mode"
fi

echo "  ── symbolic: u+w ──"
"$MODBOX" chmod u+w "$TMPDIR"/chmod_rw.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_rw.txt)
if [[ "$mode" == "200" ]]; then
    pass "chmod u+w → owner write set (mode=$mode)"
else
    fail "chmod u+w — expected mode 200, got $mode"
fi

echo "  ── symbolic: a+rw (all can read+write) ──"
echo "rwtest" > "$TMPDIR"/chmod_rwtest.txt
chmod 000 "$TMPDIR"/chmod_rwtest.txt
"$MODBOX" chmod a+rw "$TMPDIR"/chmod_rwtest.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_rwtest.txt)
if [[ "$mode" == "666" ]]; then
    pass "chmod a+rw → mode is 666"
else
    fail "chmod a+rw — expected mode 666, got $mode"
fi

echo "  ── symbolic: go-rwx (clear group/other) ──"
echo "goclear" > "$TMPDIR"/chmod_go.txt
chmod 755 "$TMPDIR"/chmod_go.txt
"$MODBOX" chmod go-rwx "$TMPDIR"/chmod_go.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_go.txt)
if [[ "$mode" == "700" ]]; then
    pass "chmod go-rwx → mode is 700"
else
    fail "chmod go-rwx — expected mode 700, got $mode"
fi

echo "  ── symbolic: a=rwx (set all) ──"
echo "alleq" > "$TMPDIR"/chmod_alleq.txt
chmod 644 "$TMPDIR"/chmod_alleq.txt
"$MODBOX" chmod a=rwx "$TMPDIR"/chmod_alleq.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_alleq.txt)
if [[ "$mode" == "777" ]]; then
    pass "chmod a=rwx → mode is 777"
else
    fail "chmod a=rwx — expected mode 777, got $mode"
fi

echo "  ── -v: verbose output ──"
"$MODBOX" chmod -v 755 "$TMPDIR"/chmod_rw.txt 2>/dev/null | grep -q "mode of"
if [[ $? -eq 0 ]]; then
    pass "chmod -v → shows mode change"
else
    fail "chmod -v — expected verbose output"
fi

echo "  ── -v: no output when mode unchanged ──"
chmod 000 "$TMPDIR"/chmod_rw.txt
out=$("$MODBOX" chmod -v 000 "$TMPDIR"/chmod_rw.txt 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "chmod -v — no output when mode unchanged"
else
    fail "chmod -v — unexpected output: [$out]"
fi

echo "  ── -c: reports change ──"
echo "cc" > "$TMPDIR"/chmod_cc.txt
chmod 644 "$TMPDIR"/chmod_cc.txt
"$MODBOX" chmod -c 755 "$TMPDIR"/chmod_cc.txt 2>/dev/null | grep -q "mode of"
if [[ $? -eq 0 ]]; then
    pass "chmod -c → reports change"
else
    fail "chmod -c — expected change report"
fi

echo "  ── -c: no output when mode unchanged ──"
out=$("$MODBOX" chmod -c 755 "$TMPDIR"/chmod_cc.txt 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "chmod -c — no output when mode unchanged"
else
    fail "chmod -c — unexpected output: [$out]"
fi

echo "  ── -f: silent (suppress error) ──"
out=$("$MODBOX" chmod -f 644 "$TMPDIR"/chmod_nonexistent 2>&1 || true)
if [[ -z "$out" ]]; then
    pass "chmod -f → no error output"
else
    fail "chmod -f — unexpected output: [$out]"
fi

echo "  ── --reference: copy mode from ref file ──"
echo "refsrc" > "$TMPDIR"/chmod_refsrc.txt
chmod 777 "$TMPDIR"/chmod_refsrc.txt
echo "reftgt" > "$TMPDIR"/chmod_reftgt.txt
chmod 644 "$TMPDIR"/chmod_reftgt.txt
"$MODBOX" chmod --reference="$TMPDIR"/chmod_refsrc.txt "$TMPDIR"/chmod_reftgt.txt 2>/dev/null || true
mode=$(stat -c '%a' "$TMPDIR"/chmod_reftgt.txt)
if [[ "$mode" == "777" ]]; then
    pass "chmod --reference → mode is 777"
else
    fail "chmod --reference — expected 777, got $mode"
fi

echo "  ── -R: recursive ──"
chmod 755 "$TMPDIR"/chmod_dir
chmod 644 "$TMPDIR"/chmod_dir/sub/file.txt
"$MODBOX" chmod -R 777 "$TMPDIR"/chmod_dir 2>/dev/null || true
dir_mode=$(stat -c '%a' "$TMPDIR"/chmod_dir)
file_mode=$(stat -c '%a' "$TMPDIR"/chmod_dir/sub/file.txt)
if [[ "$dir_mode" == "777" && "$file_mode" == "777" ]]; then
    pass "chmod -R 777 → dir=$dir_mode file=$file_mode"
else
    fail "chmod -R 777 — expected both 777, got dir=$dir_mode file=$file_mode"
fi

echo "  ── -Rv: recursive verbose ──"
chmod -R 644 "$TMPDIR"/chmod_dir
assert_cmd_pat "mode of" chmod -Rv 755 "$TMPDIR"/chmod_dir
echo "  ── -R symbolic: a-x recursively ──"
mkdir -p "$TMPDIR"/chmod_symr/sub
chmod 755 "$TMPDIR"/chmod_symr
chmod 644 "$TMPDIR"/chmod_symr/sub/file.txt
"$MODBOX" chmod -R a-x "$TMPDIR"/chmod_symr 2>/dev/null || true
dir_mode=$(stat -c '%a' "$TMPDIR"/chmod_symr)
if [[ "$dir_mode" == "644" ]]; then
    pass "chmod -R a-x → dir=$dir_mode"
else
    fail "chmod -R a-x — expected dir 644, got $dir_mode"
fi
# Restore permissions so cleanup can delete the dir tree
chmod -R u+wX "$TMPDIR"/chmod_symr 2>/dev/null || true
