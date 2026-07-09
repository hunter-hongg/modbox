echo ""
echo "── grep ────────────────────────────────────"

mkdir -p "$TMPDIR"/grep_dir
printf 'hello world\nfoo bar\nhello again\n' > "$TMPDIR"/grep_dir/a.txt
printf 'abc\n123\ndef\n' > "$TMPDIR"/grep_dir/b.txt
printf 'HELLO WORLD\nHello World\nlowercase\n' > "$TMPDIR"/grep_dir/case.txt
printf 'apple\nbanana\napple pie\npineapple\n' > "$TMPDIR"/grep_dir/words.txt
printf 'abc123def\n' > "$TMPDIR"/grep_dir/line.txt
printf 'a,b,c\n1,2,3\nx,y,z\n' > "$TMPDIR"/grep_dir/csv.txt

echo "  ── basic pattern search ──"
assert_cmd "$(printf 'hello world\nhello again\n')" grep hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -n : line numbers ──"
assert_cmd "$(printf '1:hello world\n3:hello again\n')" grep -n hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -i : case-insensitive ──"
assert_cmd "$(printf 'HELLO WORLD\nHello World\n')" grep -i hello "$TMPDIR"/grep_dir/case.txt

echo "  ── -v : invert match ──"
assert_cmd "$(printf 'foo bar\n')" grep -v hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -w : word match ──"
assert_cmd "$(printf 'apple\napple pie\n')" grep -w apple "$TMPDIR"/grep_dir/words.txt

echo "  ── -x : line-regexp ──"
assert_cmd "$(printf 'abc123def\n')" grep -x abc123def "$TMPDIR"/grep_dir/line.txt
assert_cmd "" grep -x abc "$TMPDIR"/grep_dir/line.txt

echo "  ── -E : extended regex ──"
assert_cmd "$(printf '123\n')" grep -E '[0-9]+' "$TMPDIR"/grep_dir/b.txt

echo "  ── -F : fixed strings ──"
printf 'hello.world\n' > "$TMPDIR"/grep_dir/fixed.txt
assert_cmd "hello.world" grep -F "hello." "$TMPDIR"/grep_dir/fixed.txt

echo "  ── -c : count ──"
assert_cmd "2" grep -c hello "$TMPDIR"/grep_dir/a.txt
assert_cmd "0" grep -c hello "$TMPDIR"/grep_dir/b.txt

echo "  ── -l : files with matches ──"
assert_cmd "$(printf '%s\n' "$TMPDIR"/grep_dir/a.txt)" grep -l hello "$TMPDIR"/grep_dir/a.txt "$TMPDIR"/grep_dir/b.txt

echo "  ── -H : always show filename ──"
assert_cmd_pat 'a\.txt:hello' grep -H hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -h : never show filename ──"
assert_cmd "$(printf '1:hello world\n3:hello again\n')" grep -h -n hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -o : only-matching ──"
assert_cmd "$(printf 'hello\nhello\n')" grep -o hello "$TMPDIR"/grep_dir/a.txt

echo "  ── stdin ──"
assert_cmd "hello" grep hello <<<"hello"

echo "  ── -e : pattern from flag ──"
assert_cmd "hello" grep -e hello <<<"hello"

echo "  ── --color=always : ANSI escape codes present ──"
assert_cmd_pat $'\x1b\\[' grep --color=always hello "$TMPDIR"/grep_dir/a.txt

echo "  ── -r : recursive search ──"
mkdir -p "$TMPDIR"/grep_dir/subdir
printf 'deep match\n' > "$TMPDIR"/grep_dir/subdir/deep.txt
assert_cmd_pat 'deep\.txt:deep match' grep -r deep "$TMPDIR"/grep_dir/subdir

echo "  ── exit code 0 on match ──"
if "$MODBOX" grep hello "$TMPDIR"/grep_dir/a.txt >/dev/null 2>&1; then
    pass "grep hello → exit 0"
else
    fail "grep hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" grep nonexistent "$TMPDIR"/grep_dir/a.txt >/dev/null 2>&1; then
    fail "grep nonexistent → expected exit 1"
else
    pass "grep nonexistent → exit 1"
fi

echo "  ── error: no pattern ──"
if "$MODBOX" grep >/dev/null 2>&1; then
    fail "grep (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "grep (no args) → exit 2"
    else
        fail "grep (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── stdin with -n ──"
assert_cmd "1:hello" grep -n hello <<<"hello"

echo "  ── -w with -F : word match fixed strings ──"
printf 'foobar foo bar\n' > "$TMPDIR"/grep_dir/wordfixed.txt
assert_cmd "foobar foo bar" grep -wF foo "$TMPDIR"/grep_dir/wordfixed.txt
# "foobar" should NOT match "foo" as a word
assert_cmd "" grep -wF foobar <<<"foo"
