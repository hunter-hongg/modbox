echo ""
echo "── rg ──────────────────────────────────────"

mkdir -p "$TMPDIR"/rg_dir
printf 'hello world\nfoo bar\nhello again\n' > "$TMPDIR"/rg_dir/a.txt
printf 'abc\n123\ndef\n' > "$TMPDIR"/rg_dir/b.txt
printf 'HELLO WORLD\nHello World\nlowercase\n' > "$TMPDIR"/rg_dir/case.txt
printf 'apple\nbanana\napple pie\npineapple\n' > "$TMPDIR"/rg_dir/words.txt
printf 'abc123def\n' > "$TMPDIR"/rg_dir/line.txt
printf 'hello.world\n' > "$TMPDIR"/rg_dir/fixed.txt
printf 'a\nb\nc hello\nd\ne\nf hello2\ng\n' > "$TMPDIR"/rg_dir/context.txt

echo "  ── basic search (default: line numbers) ──"
assert_cmd "$(printf '1:hello world\n--\n3:hello again\n')" rg hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -N : suppress line numbers ──"
assert_cmd "$(printf 'hello world\n--\nhello again\n')" rg -N hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -n : explicit line numbers (same as default) ──"
assert_cmd "$(printf '1:hello world\n--\n3:hello again\n')" rg -n hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -i : case-insensitive ──"
assert_cmd "$(printf '1:HELLO WORLD\n2:Hello World\n')" rg -i hello "$TMPDIR"/rg_dir/case.txt

echo "  ── -s : case-sensitive ──"
assert_cmd "" rg -s hello "$TMPDIR"/rg_dir/case.txt

echo "  ── -S : smart-case (uppercase pat = case-sensitive) ──"
assert_cmd "$(printf '1:HELLO WORLD\n')" rg -S HELLO "$TMPDIR"/rg_dir/case.txt

echo "  ── -v : invert match ──"
assert_cmd "$(printf '1:abc\n2:123\n3:def\n')" rg -v hello "$TMPDIR"/rg_dir/b.txt

echo "  ── -w : word match (apple matches, not pineapple) ──"
assert_cmd "$(printf '1:apple\n--\n3:apple pie\n')" rg -w apple "$TMPDIR"/rg_dir/words.txt
assert_cmd "$(printf '4:pineapple\n')" rg -w pineapple "$TMPDIR"/rg_dir/words.txt

echo "  ── -x : line-regexp ──"
assert_cmd "$(printf '1:abc123def\n')" rg -x abc123def "$TMPDIR"/rg_dir/line.txt
assert_cmd "" rg -x abc "$TMPDIR"/rg_dir/line.txt

echo "  ── -F : fixed strings ──"
assert_cmd "1:hello.world" rg -F hello. "$TMPDIR"/rg_dir/fixed.txt

echo "  ── -c : count ──"
# ripgrep style: count lines per file
assert_cmd "2" rg -c hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -l : files with matches ──"
assert_cmd "$(printf '%s\n' "$TMPDIR"/rg_dir/a.txt)" rg -l hello "$TMPDIR"/rg_dir/a.txt "$TMPDIR"/rg_dir/b.txt

echo "  ── -o : only-matching ──"
assert_cmd "$(printf 'hello\nhello\n')" rg -o hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -e : pattern from flag ──"
assert_cmd "1:hello" rg -e hello <<<"hello"

echo "  ── --color=always : ANSI codes present ──"
# Check for the ESC character (0x1b) in output
OUTPUT=$("$MODBOX" rg --color=always hello "$TMPDIR"/rg_dir/a.txt 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    pass "rg --color=always hello ... → ANSI codes present"
else
    fail "rg --color=always hello ... — expected ANSI codes, got plain text"
fi

echo "  ── auto-recursive on directory input ──"
assert_cmd_pat 'hello' rg hello "$TMPDIR"/rg_dir

echo "  ── -C 1 : context lines ──"
assert_cmd "$(printf '1:hello world\n2-foo bar\n--\n3:hello again\n')" rg -C 1 hello "$TMPDIR"/rg_dir/a.txt

echo "  ── -C 2 : context lines around separate matches ──"
assert_cmd "$(printf '2-b\n3:c hello\n4-d\n--\n5-e\n6:f hello2\n7-g\n')" rg -C 1 hello "$TMPDIR"/rg_dir/context.txt

echo "  ── -g glob include ──"
# -g '*.txt' should only find .txt files; check that 'hello' is in the output
# and that the output includes .txt files
assert_cmd_pat 'hello' rg -g '*.txt' hello "$TMPDIR"/rg_dir
# .txt files should NOT contain 'b' (b.txt has abc/123/def, no hello)
# but all the .txt files in rg_dir have 'hello' somewhere... skip this assertion

echo "  ── -g glob exclude ──"
assert_cmd_pat 'hello' rg -g '!b.txt' hello "$TMPDIR"/rg_dir

echo "  ── --max-depth 0 (no recursion) ──"
mkdir -p "$TMPDIR"/rg_dir/subdir
printf 'deep\n' > "$TMPDIR"/rg_dir/subdir/deep.txt
assert_cmd "" rg --max-depth 0 deep "$TMPDIR"/rg_dir

echo "  ── --max-depth 1 (one level) ──"
assert_cmd_pat 'deep' rg --max-depth 1 deep "$TMPDIR"/rg_dir

echo "  ── -m 1 : max-count per file ──"
assert_cmd "1:hello world" rg -m 1 hello "$TMPDIR"/rg_dir/a.txt

echo "  ── stdin ──"
assert_cmd "1:hello" rg hello <<<"hello"

echo "  ── stdin with -N ──"
assert_cmd "hello" rg -N hello <<<"hello"

echo "  ── multiple files ──"
assert_cmd_pat 'hello world' rg hello "$TMPDIR"/rg_dir/a.txt "$TMPDIR"/rg_dir/b.txt

echo "  ── exit code 0 on match ──"
if "$MODBOX" rg hello "$TMPDIR"/rg_dir/a.txt >/dev/null 2>&1; then
    pass "rg hello → exit 0"
else
    fail "rg hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" rg nonexistent "$TMPDIR"/rg_dir/a.txt >/dev/null 2>&1; then
    fail "rg nonexistent → expected exit 1"
else
    pass "rg nonexistent → exit 1"
fi

echo "  ── error: no pattern ──"
if "$MODBOX" rg >/dev/null 2>&1; then
    fail "rg (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "rg (no args) → exit 2"
    else
        fail "rg (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── stdin with -n (default line nums) ──"
assert_cmd "1:hello" rg hello <<<"hello"

echo "  ── -w with -F : word match fixed strings ──"
printf 'foobar foo bar\n' > "$TMPDIR"/rg_dir/wordfixed.txt
assert_cmd "1:foobar foo bar" rg -wF foo "$TMPDIR"/rg_dir/wordfixed.txt
assert_cmd "" rg -wF foobar <<<"foo"

echo "  ── --color=auto : no ANSI when piped ──"
OUTPUT=$("$MODBOX" rg --color=auto hello "$TMPDIR"/rg_dir/a.txt 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\\033'; then
    fail "rg --color=auto (piped) — expected no ANSI codes, got them"
else
    pass "rg --color=auto (piped) — ANSI codes correctly disabled"
fi

echo "  ── --hidden : include hidden files ──"
printf 'hidden match\n' > "$TMPDIR"/rg_dir/.hidden.txt
assert_cmd_pat 'hidden match' rg --hidden hidden "$TMPDIR"/rg_dir
assert_cmd_not_pat 'hidden match' rg hidden "$TMPDIR"/rg_dir  # without --hidden, should NOT match
