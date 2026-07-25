SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── wc ───────────────────────────────────────"

printf 'hello world\nfoo bar baz\n' > "$TMPDIR"/wc_test.txt

echo "  ── default (lines words bytes) ──"
assert_cmd "$(printf '       2       5      24 %s/wc_test.txt' "$TMPDIR")" wc "$TMPDIR"/wc_test.txt

echo "  ── -l (lines) ──"
assert_cmd "$(printf '       2 %s/wc_test.txt' "$TMPDIR")" wc -l "$TMPDIR"/wc_test.txt

echo "  ── -w (words) ──"
assert_cmd "$(printf '       5 %s/wc_test.txt' "$TMPDIR")" wc -w "$TMPDIR"/wc_test.txt

echo "  ── -c (bytes) ──"
assert_cmd "$(printf '      24 %s/wc_test.txt' "$TMPDIR")" wc -c "$TMPDIR"/wc_test.txt

echo "  ── combined -cwl ──"
assert_cmd "$(printf '       2       5      24 %s/wc_test.txt' "$TMPDIR")" wc -cwl "$TMPDIR"/wc_test.txt

echo "  ── -m (chars) same as -c here ──"
assert_cmd "$(printf '      24 %s/wc_test.txt' "$TMPDIR")" wc -m "$TMPDIR"/wc_test.txt

echo "  ── multiple files with total ──"
assert_cmd "$(printf '       2 %s/wc_test.txt\n       2 %s/wc_test.txt\n       4 total' "$TMPDIR" "$TMPDIR")" wc -l "$TMPDIR"/wc_test.txt "$TMPDIR"/wc_test.txt

echo "  ── stdin ──"
result=$(printf 'a b\nc d e\n' | "$MODBOX" wc 2>/dev/null || true)
if [[ "$result" == "$(printf '       2       5      10')" ]]; then
    pass "wc (stdin)"
else
    fail "wc (stdin) — expected [2 5 10] got [$result]"
fi

echo "  ── - (stdin dash) ──"
result=$(printf 'x y z\n' | "$MODBOX" wc -l - 2>/dev/null || true)
if [[ "$result" == "$(printf '       1 -')" ]]; then
    pass "wc - (stdin dash)"
else
    fail "wc - (stdin dash) — expected [1 -] got [$result]"
fi

echo "  ── empty file ──"
: > "$TMPDIR"/wc_empty.txt
assert_cmd "$(printf '       0       0       0 %s/wc_empty.txt' "$TMPDIR")" wc "$TMPDIR"/wc_empty.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' wc --help

echo "  ── --json single file ──"
output=$("$MODBOX" wc --json "$TMPDIR"/wc_test.txt 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    lines_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['lines'])")
    words_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['words'])")
    bytes_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['bytes'])")
    chars_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['chars'])")
    name_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['name'])")
    if [[ "$lines_val" == "2" && "$words_val" == "5" && "$bytes_val" == "24" && "$chars_val" == "24" ]]; then
        pass "wc --json single file (valid JSON, correct counts)"
    else
        fail "wc --json single file — expected [2 5 24 24] got [$lines_val $words_val $bytes_val $chars_val]"
    fi
else
    fail "wc --json single file — output is not valid JSON: $output"
fi

echo "  ── --json multiple files with total ──"
printf 'one two three\n' > "$TMPDIR"/wc_test_a.txt
printf 'four five\n' > "$TMPDIR"/wc_test_b.txt
output=$("$MODBOX" wc --json "$TMPDIR"/wc_test_a.txt "$TMPDIR"/wc_test_b.txt 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    count=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d))")
    total_words=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); t=[x for x in d if x.get('name')=='total']; print(t[0]['words'] if t else -1)")
    if [[ "$count" == "3" && "$total_words" == "5" ]]; then
        pass "wc --json multiple files (3 entries + total)"
    else
        fail "wc --json multiple files — expected 3 entries and total_words=5, got count=$count total_words=$total_words"
    fi
else
    fail "wc --json multiple files — output is not valid JSON: $output"
fi

echo "  ── --json stdin ──"
output=$(echo "hello world foo" | "$MODBOX" wc --json 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    lines_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['lines'])")
    name_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['name'])")
    if [[ "$lines_val" == "1" && "$name_val" == "" ]]; then
        pass "wc --json stdin (empty name)"
    else
        fail "wc --json stdin — expected lines=1 name='', got lines=$lines_val name='$name_val'"
    fi
else
    fail "wc --json stdin — output is not valid JSON: $output"
fi

echo "  ── --json stdin dash ──"
output=$(echo "hello" | "$MODBOX" wc --json - 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    name_val=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d[0]['name'])")
    if [[ "$name_val" == "-" ]]; then
        pass "wc --json stdin dash (name='-')"
    else
        fail "wc --json stdin dash — expected name='-', got '$name_val'"
    fi
else
    fail "wc --json stdin dash — output is not valid JSON: $output"
fi

echo "  ── --json empty file ──"
output=$("$MODBOX" wc --json "$TMPDIR"/wc_empty.txt 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    all_zero=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin)[0]; print(all(v==0 for k,v in d.items() if k != 'name'))")
    if [[ "$all_zero" == "True" ]]; then
        pass "wc --json empty file (all counts zero)"
    else
        fail "wc --json empty file — expected all zero counts"
    fi
else
    fail "wc --json empty file — output is not valid JSON: $output"
fi

echo "  ── --json missing file ──"
output=$("$MODBOX" wc --json "$TMPDIR"/nonexistent_file_xyz.txt 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    has_error=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print('error' in d[0])")
    if [[ "$has_error" == "True" ]]; then
        pass "wc --json missing file (error entry)"
    else
        fail "wc --json missing file — expected error entry"
    fi
else
    fail "wc --json missing file — output is not valid JSON: $output"
fi

echo "  ── --json mixed valid/invalid ──"
output=$("$MODBOX" wc --json "$TMPDIR"/wc_test.txt "$TMPDIR"/nonexistent_file_xyz.txt 2>/dev/null)
if echo "$output" | python3 -m json.tool >/dev/null 2>&1; then
    arr_len=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d))")
    has_count=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(any('lines' in x for x in d))")
    has_err=$(echo "$output" | python3 -c "import sys,json; d=json.load(sys.stdin); print(any('error' in x for x in d))")
    if [[ "$arr_len" == "2" && "$has_count" == "True" && "$has_err" == "True" ]]; then
        pass "wc --json mixed valid/invalid (count + error entries)"
    else
        fail "wc --json mixed — expected 2 entries with both count and error, got len=$arr_len count=$has_count err=$has_err"
    fi
else
    fail "wc --json mixed — output is not valid JSON: $output"
fi

echo "  ── --json key ordering ──"
output=$("$MODBOX" wc --json "$TMPDIR"/wc_test.txt 2>/dev/null)
first_key=$(echo "$output" | grep -o '"[^"]*":' | head -1 | tr -d '":')
if [[ "$first_key" == "bytes" ]]; then
    pass "wc --json keys start with 'bytes' (alphabetical order)"
else
    fail "wc --json first key is '$first_key', expected 'bytes'"
fi
