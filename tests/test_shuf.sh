SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── shuf ──────────────────────────────────────"

shuf_data="$TMPDIR/shuf_data.txt"
printf 'one\ntwo\nthree\nfour\nfive\n' > "$shuf_data"

echo "  ── -e echo mode (permutation check) ──"
result=$("$MODBOX" shuf -e a b c 2>/dev/null || true)
sorted=$(echo "$result" | sort)
expected="$(printf 'a\nb\nc')"
if [[ "$sorted" == "$expected" ]]; then
    pass "shuf -e a b c"
else
    fail "shuf -e a b c — sorted output expected [$expected] got [$sorted]"
fi

echo "  ── -e -n head-count ──"
result=$("$MODBOX" shuf -e a b c d e -n 3 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 3 ]]; then
    pass "shuf -e a b c d e -n 3 (count=$count)"
else
    fail "shuf -e a b c d e -n 3 — expected 3 lines, got $count"
fi
# Verify all lines come from input set
while IFS= read -r line; do
    case "$line" in
        a|b|c|d|e) ;;
        *) fail "shuf -e -n 3 — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── -e -r -n repeat mode ──"
result=$("$MODBOX" shuf -e x y z -r -n 8 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 8 ]]; then
    pass "shuf -e x y z -r -n 8 (count=$count)"
else
    fail "shuf -e x y z -r -n 8 — expected 8 lines, got $count"
fi
while IFS= read -r line; do
    case "$line" in
        x|y|z) ;;
        *) fail "shuf -e -r -n 8 — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── -i input-range -n head-count ──"
result=$("$MODBOX" shuf -i 1-100 -n 10 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 10 ]]; then
    pass "shuf -i 1-100 -n 10 (count=$count)"
else
    fail "shuf -i 1-100 -n 10 — expected 10 lines, got $count"
fi
while IFS= read -r line; do
    if [[ "$line" -lt 1 || "$line" -gt 100 ]]; then
        fail "shuf -i 1-100 -n 10 — value [$line] out of range"
    fi
done <<< "$result"

echo "  ── -i input-range permutation ──"
result=$("$MODBOX" shuf -i 1-5 2>/dev/null || true)
sorted=$(echo "$result" | sort -n)
expected="$(printf '1\n2\n3\n4\n5')"
if [[ "$sorted" == "$expected" ]]; then
    pass "shuf -i 1-5"
else
    fail "shuf -i 1-5 — sorted output expected [$expected] got [$sorted]"
fi

echo "  ── stdin (pipe) ──"
result=$(printf 'alpha\nbeta\ngamma\n' | "$MODBOX" shuf -n 2 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 2 ]]; then
    pass "shuf -n 2 (stdin, count=$count)"
else
    fail "shuf -n 2 (stdin) — expected 2 lines, got $count"
fi
while IFS= read -r line; do
    case "$line" in
        alpha|beta|gamma) ;;
        *) fail "shuf (stdin) — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── file input ──"
result=$("$MODBOX" shuf -n 3 "$shuf_data" 2>/dev/null || true)
count=$(echo "$result" | wc -l)
if [[ "$count" -eq 3 ]]; then
    pass "shuf -n 3 $shuf_data (count=$count)"
else
    fail "shuf -n 3 $shuf_data — expected 3 lines, got $count"
fi
while IFS= read -r line; do
    case "$line" in
        one|two|three|four|five) ;;
        *) fail "shuf file — unexpected line [$line]" ;;
    esac
done <<< "$result"

echo "  ── -o output file ──"
"$MODBOX" shuf -e p q r -o "$TMPDIR"/shuf_out.txt 2>/dev/null || true
if [[ -f "$TMPDIR"/shuf_out.txt ]]; then
    result=$(cat "$TMPDIR"/shuf_out.txt)
    sorted=$(echo "$result" | sort)
    expected="$(printf 'p\nq\nr')"
    if [[ "$sorted" == "$expected" ]]; then
        pass "shuf -o output file"
    else
        fail "shuf -o output file — sorted output expected [$expected] got [$sorted]"
    fi
else
    fail "shuf -o output file — file not created"
fi

echo "  ── -e no args (error) ──"
assert_cmd_pat_stderr 'no input lines' shuf -e

echo "  ── -e -i conflict (error) ──"
assert_cmd_pat_stderr 'cannot combine' shuf -e -i 1-5

echo "  ── -i + file conflict (error) ──"
assert_cmd_pat_stderr 'cannot combine' shuf -i 1-5 "$shuf_data"

echo "  ── -i invalid range (error) ──"
assert_cmd_pat_stderr 'invalid input range' shuf -i 5-3

echo "  ── non-existent file (error) ──"
assert_cmd_pat_stderr 'No such file' shuf "$TMPDIR"/shuf_nonexistent

echo "  ── help ──"
assert_cmd_pat 'Usage:' shuf --help
