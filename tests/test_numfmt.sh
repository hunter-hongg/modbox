SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── numfmt ────────────────────────────────────"

echo "  ── basic passthrough (no scaling)"
assert_cmd "1000" numfmt 1000

echo "  ── --to=si"
assert_cmd "1.0K" numfmt --to=si 1000

echo "  ── --to=si large"
assert_cmd "1.0M" numfmt --to=si 1000000

echo "  ── --to=iec"
assert_cmd "1.0K" numfmt --to=iec 1024

echo "  ── --to=iec-i"
assert_cmd "1.0Mi" numfmt --to=iec-i 1048576

echo "  ── --from=si"
assert_cmd "1000" numfmt --from=si 1K

echo "  ── --from=iec"
assert_cmd "1048576" numfmt --from=iec 1M

echo "  ── --from=auto SI"
assert_cmd "1000" numfmt --from=auto 1K

echo "  ── --from=auto IEC"
assert_cmd "1024" numfmt --from=auto 1Ki

echo "  ── --from=si --to=iec"
assert_cmd "977K" numfmt --from=si --to=iec 1M

echo "  ── --padding=10 right-align"
assert_cmd "      1.0K" numfmt --to=si --padding=10 1000

echo "  ── --padding=-10 left-align"
assert_cmd "1.0K      " numfmt --to=si --padding=-10 1000

echo "  ── --suffix=B"
assert_cmd "1.0KB" numfmt --to=si --suffix=B 1000

echo "  ── --round=nearest"
assert_cmd "1.5K" numfmt --to=si --round=nearest 1500

echo "  ── --round=down"
assert_cmd "1.5K" numfmt --to=si --round=down 1500

echo "  ── --round=from-zero (rounds 1.35 up to 1.4)"
assert_cmd "1.4K" numfmt --to=si --round=from-zero 1350

echo "  ── --round=down (rounds 1.9K down)"
assert_cmd "1.9K" numfmt --to=si --round=down 1999

echo "  ── --from-unit=1024"
assert_cmd "1048576" numfmt --from-unit=1024 1024

echo "  ── --to-unit=1024"
assert_cmd "1" numfmt --to-unit=1024 1024

echo "  ── invalid number error"
assert_cmd_pat_stderr 'invalid' numfmt abc

echo "  ── --invalid=ignore"
assert_cmd "abc" numfmt --invalid=ignore abc

echo "  ── help"
assert_cmd_pat 'Usage:' numfmt --help

echo "  ── stdin processing"
actual=$(echo "1000" | "$MODBOX" numfmt --to=si 2>/dev/null || true)
if [[ "$actual" == "1.0K" ]]; then pass "stdin --to=si"; else fail "stdin --to=si — expected [1.0K] got [$actual]"; fi
