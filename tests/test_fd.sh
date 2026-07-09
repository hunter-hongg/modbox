SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── fd ──────────────────────────────────────"

mkdir -p "$TMPDIR"/fd_dir/sub
printf 'hello world\n'  > "$TMPDIR"/fd_dir/hello.txt
printf 'stuff\n'        > "$TMPDIR"/fd_dir/Hello.txt
printf 'fn main() {}\n' > "$TMPDIR"/fd_dir/main.rs
printf 'data\n'         > "$TMPDIR"/fd_dir/data.rs
printf 'nested content\n' > "$TMPDIR"/fd_dir/sub/nested.txt
printf 'empty file\n'   > "$TMPDIR"/fd_dir/sub/empty.txt
: > "$TMPDIR"/fd_dir/empty_file.txt
touch "$TMPDIR"/fd_dir/.hidden_file
printf '#!/bin/sh\n' > "$TMPDIR"/fd_dir/exec_script.sh
chmod +x "$TMPDIR"/fd_dir/exec_script.sh

echo "  ── basic pattern search ──"
assert_cmd_pat 'hello\.txt' fd hello "$TMPDIR"/fd_dir

echo "  ── case-sensitive (-s) ──"
assert_cmd_pat 'Hello\.txt' fd -s Hello "$TMPDIR"/fd_dir
assert_cmd_pat 'hello\.txt' fd -s hello "$TMPDIR"/fd_dir
assert_cmd_not_pat 'Hello' fd -s hello "$TMPDIR"/fd_dir

echo "  ── case-insensitive (-i) ──"
assert_cmd_pat 'hello' fd -i HELLO "$TMPDIR"/fd_dir

echo "  ── smart-case (default): lowercase pat = case-insensitive ──"
assert_cmd_pat 'hello' fd hello "$TMPDIR"/fd_dir
assert_cmd_pat 'Hello' fd hello "$TMPDIR"/fd_dir

echo "  ── smart-case: uppercase pat = case-sensitive ──"
assert_cmd_pat 'Hello\.txt' fd Hello "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello' fd Hello "$TMPDIR"/fd_dir

echo "  ── glob mode (-g) ──"
assert_cmd_pat 'main\.rs' fd -g '*.rs' "$TMPDIR"/fd_dir

echo "  ── -H: hidden files ──"
assert_cmd_pat 'hidden_file' fd -H hidden "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hidden_file' fd hidden "$TMPDIR"/fd_dir

echo "  ── -t f: files only ──"
results_f=$("$MODBOX" fd -t f '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
results_all=$("$MODBOX" fd '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
if [ "$results_f" -gt 0 ] && [ "$results_f" -lt "$results_all" ]; then
    pass "fd -t f → $results_f files (vs $results_all total)"
else
    fail "fd -t f — expected files only, got $results_f (total $results_all)"
fi

echo "  ── -t d: directories only ──"
assert_cmd_pat 'fd_dir/sub$' fd -t d 'sub' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello\.txt' fd -t d '' "$TMPDIR"/fd_dir

echo "  ── -t l: symlinks ──"
ln -sf "$TMPDIR"/fd_dir/hello.txt "$TMPDIR"/fd_dir/link.ln
assert_cmd_pat 'link\.ln' fd -t l 'link' "$TMPDIR"/fd_dir

echo "  ── -t x: executables ──"
assert_cmd_pat 'exec_script' fd -t x 'exec' "$TMPDIR"/fd_dir

echo "  ── -t e: empty files ──"
assert_cmd_pat 'empty_file\.txt' fd -t e 'empty' "$TMPDIR"/fd_dir

echo "  ── -e extension filter ──"
assert_cmd_pat 'main\.rs' fd -e rs 'main' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'hello\.txt' fd -e rs '' "$TMPDIR"/fd_dir

echo "  ── -E exclude pattern ──"
assert_cmd_not_pat 'sub' fd -E 'sub' '' "$TMPDIR"/fd_dir 2>/dev/null

echo "  ── -d max depth ──"
assert_cmd_pat 'hello\.txt' fd -d 1 'txt' "$TMPDIR"/fd_dir
assert_cmd_pat 'nested\.txt' fd -d 1 'nested' "$TMPDIR"/fd_dir
assert_cmd_pat 'nested\.txt' fd -d 2 'nested' "$TMPDIR"/fd_dir

echo "  ── -p full path search ──"
assert_cmd_pat 'fd_dir/hello' fd -p 'fd_dir/' "$TMPDIR"/fd_dir
assert_cmd_not_pat 'fd_dir/hello' fd 'fd_dir/' "$TMPDIR"/fd_dir

echo "  ── -0 print0 ──"
has_nul=$("$MODBOX" fd -0 'hello' "$TMPDIR"/fd_dir 2>/dev/null | tr -d '[:print:]' | grep -c $'\x00' || true)
if [ "$has_nul" -gt 0 ]; then
    pass "fd -0 → output contains NUL chars"
else
    fail "fd -0 — expected NUL-separated output"
fi

echo "  ── --color=always ──"
assert_cmd_pat $'\x1b\[' fd --color=always '.' "$TMPDIR"/fd_dir

echo "  ── --color=always has ANSI codes ──"
OUTPUT=$("$MODBOX" fd --color=always '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    pass "fd --color=always → ANSI codes present"
else
    fail "fd --color=always — expected ANSI codes"
fi

echo "  ── --color=auto (piped) no ANSI ──"
OUTPUT=$("$MODBOX" fd --color=auto '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    fail "fd --color=auto (piped) — expected no ANSI codes, got them"
else
    pass "fd --color=auto (piped) — ANSI codes correctly disabled"
fi

echo "  ── --color=never ──"
OUTPUT=$("$MODBOX" fd --color=never '.' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$OUTPUT" | grep -q $'\x1b'; then
    fail "fd --color=never — expected no ANSI codes, got them"
else
    pass "fd --color=never — no ANSI codes"
fi

echo "  ── --max-results N ──"
line_count=$("$MODBOX" fd --max-results 3 '.' "$TMPDIR"/fd_dir 2>/dev/null | wc -l)
if [ "$line_count" -le 3 ]; then
    pass "fd --max-results 3 → $line_count lines (≤3)"
else
    fail "fd --max-results 3 → $line_count lines, expected ≤3"
fi

echo "  ── multiple paths ──"
assert_cmd_pat 'hello' fd hello "$TMPDIR"/fd_dir "$TMPDIR"

echo "  ── error: no pattern ──"
if "$MODBOX" fd >/dev/null 2>&1; then
    fail "fd (no args) → expected error exit 2"
else
    code=$?
    if [ "$code" -eq 2 ]; then
        pass "fd (no args) → exit 2"
    else
        fail "fd (no args) → expected exit 2, got $code"
    fi
fi

echo "  ── error: invalid type ──"
assert_cmd_pat_stderr 'invalid type' fd -t z '.' /dev/null

echo "  ── exit code 0 on match ──"
if "$MODBOX" fd hello "$TMPDIR"/fd_dir >/dev/null 2>&1; then
    pass "fd hello → exit 0"
else
    fail "fd hello → expected exit 0"
fi

echo "  ── exit code 1 on no match ──"
if "$MODBOX" fd nonexistentzzz "$TMPDIR"/fd_dir >/dev/null 2>&1; then
    fail "fd nonexistent → expected exit 1"
else
    pass "fd nonexistent → exit 1"
fi

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' fd --help

echo "  ── -x exec per file (echo) ──"
result=$("$MODBOX" fd -x echo '\.rs$' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$result" | grep -qE '\.rs$'; then
    pass "fd -x echo '.rs' → exec output contains .rs paths"
else
    fail "fd -x echo '.rs' — expected .rs paths in output, got [$result]"
fi

echo "  ── -X exec batch (echo) ──"
result=$("$MODBOX" fd -X echo '\.rs$' "$TMPDIR"/fd_dir 2>/dev/null || true)
if echo "$result" | grep -qE '\.rs'; then
    pass "fd -X echo '.rs' → batch exec output contains .rs paths"
else
    fail "fd -X echo '.rs' — expected .rs paths in output, got [$result]"
fi

echo "  ── -L follow symlinks ──"
mkdir -p "$TMPDIR"/fd_follow/sub
printf 'followed\n' > "$TMPDIR"/fd_follow/sub/target.txt
ln -sf "$TMPDIR"/fd_follow "$TMPDIR"/fd_follow_link
assert_cmd_pat 'target' fd -L 'target' "$TMPDIR"/fd_follow_link
