SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── zcat ──────────────────────────────────────"

echo "  ── help / version ──"
assert_cmd_pat 'Usage:' zcat --help
assert_cmd_pat 'zcat \(modbox\) 1\.0' zcat --version


echo "  ── single file decompress to stdout (T01) ──"
printf 'hello zcat\n' > "$TMPDIR/z1.txt"
"$MODBOX" gzip "$TMPDIR/z1.txt" >/dev/null 2>&1
out=$("$MODBOX" zcat "$TMPDIR/z1.txt.gz" 2>/dev/null)
if [[ "$out" == "hello zcat" ]]; then pass "zcat: decompresses single file to stdout"; else fail "zcat: got [$out] expected 'hello zcat'"; fi
# Verify original file still exists
if [[ -f "$TMPDIR/z1.txt.gz" ]]; then pass "zcat: keeps original .gz file"; else fail "zcat: original .gz was removed"; fi


echo "  ── multi-file decompress (T02) ──"
printf 'file one\n' > "$TMPDIR/m1.txt"
printf 'file two\n' > "$TMPDIR/m2.txt"
printf 'file three\n' > "$TMPDIR/m3.txt"
"$MODBOX" gzip "$TMPDIR/m1.txt" "$TMPDIR/m2.txt" "$TMPDIR/m3.txt" >/dev/null 2>&1
out=$("$MODBOX" zcat "$TMPDIR/m1.txt.gz" "$TMPDIR/m2.txt.gz" "$TMPDIR/m3.txt.gz" 2>/dev/null)
expected="file one
file two
file three"
if [[ "$out" == "$expected" ]]; then pass "zcat: concatenates multiple files"; else fail "zcat: multi-file output mismatch"; fi


echo "  ── stdin/stdout (T03) ──"
printf 'stdin data' > "$TMPDIR/stdin.txt"
"$MODBOX" gzip "$TMPDIR/stdin.txt" >/dev/null 2>&1
out=$(cat "$TMPDIR/stdin.txt.gz" | "$MODBOX" zcat 2>/dev/null)
if [[ "$out" == "stdin data" ]]; then pass "zcat: reads from stdin"; else fail "zcat: stdin got [$out]"; fi

# Test with - as explicit stdin
out=$("$MODBOX" zcat - < "$TMPDIR/stdin.txt.gz" 2>/dev/null)
if [[ "$out" == "stdin data" ]]; then pass "zcat: - reads from stdin"; else fail "zcat: - flag got [$out]"; fi


echo "  ── error handling (T04) ──"
"$MODBOX" zcat "$TMPDIR/does_not_exist_xyz.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zcat: missing file exits non-zero"; else fail "zcat: missing file should exit non-zero"; fi
assert_cmd_pat_stderr 'No such file' zcat "$TMPDIR/does_not_exist_xyz.txt"

# corrupt/truncated .gz
printf '\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x00\x00garbage' > "$TMPDIR/bad.gz"
"$MODBOX" zcat "$TMPDIR/bad.gz" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zcat: corrupt .gz exits non-zero"; else fail "zcat: corrupt .gz should fail"; fi

# non-gzip file
printf 'hello not gzip\n' > "$TMPDIR/plain.txt"
"$MODBOX" zcat "$TMPDIR/plain.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zcat: non-gzip exits non-zero"; else fail "zcat: non-gzip should fail"; fi
assert_cmd_pat_stderr 'not in gzip format' zcat "$TMPDIR/plain.txt"


echo "  ── empty file handling (T05) ──"
: > "$TMPDIR/empty.txt"
"$MODBOX" gzip "$TMPDIR/empty.txt" >/dev/null 2>&1
out=$("$MODBOX" zcat "$TMPDIR/empty.txt.gz" 2>/dev/null)
if [[ -z "$out" ]]; then pass "zcat: empty .gz decompresses to empty output"; else fail "zcat: empty output got [$out]"; fi


echo "  ── exit code with multiple files (T06) ──"
printf 'good file\n' > "$TMPDIR/good.txt"
"$MODBOX" gzip "$TMPDIR/good.txt" >/dev/null 2>&1
"$MODBOX" zcat "$TMPDIR/does_not_exist_xyz.txt" "$TMPDIR/good.txt.gz" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zcat: mix of bad+good exits non-zero"; else fail "zcat: mix should exit non-zero"; fi