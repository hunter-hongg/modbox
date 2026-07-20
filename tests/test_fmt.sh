SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── fmt ──────────────────────────────────────"

echo " ── basic format ──"
printf "hello world\n" > "$TMPDIR"/fmt_in
assert_cmd_pat "hello world" "$MODBOX" fmt --width=79 "$TMPDIR"/fmt_in

echo " ── long line wrap ──"
printf '%.0sx' {1..70} > "$TMPDIR"/fmt_long
printf '\n%.0sx' {1..70} >> "$TMPDIR"/fmt_long
result=$("$MODBOX" fmt --width=10 "$TMPDIR"/fmt_long 2>/dev/null || true)
echo "$result" | grep -qE "x{10}" && pass "fmt long lines wrap (width 10)" || fail "fmt long line wrap — output: [$result]"

echo " ── help ──"
assert_cmd_pat 'Usage:' "$MODBOX" fmt --help
