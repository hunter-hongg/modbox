SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── mkdir ────────────────────────────────────"

echo "  ── basic create ──"
assert_cmd "" mkdir "$TMPDIR"/mkdir_new

echo "  ── already exists ──"
assert_cmd_pat_stderr "File exists" mkdir "$TMPDIR"/mkdir_new

echo "  ── -p parents ──"
assert_cmd "" mkdir -p "$TMPDIR"/mkdir_a/mkdir_b/mkdir_c

echo "  ── -p existing ──"
assert_cmd "" mkdir -p "$TMPDIR"/mkdir_new

echo "  ── -v verbose ──"
assert_cmd_pat "created directory" mkdir -v "$TMPDIR"/mkdir_verb

echo "  ── -m mode ──"
assert_cmd "" mkdir -m 0700 "$TMPDIR"/mkdir_mode
# Verify mode
mode=$(stat -c '%a' "$TMPDIR"/mkdir_mode 2>/dev/null)
if [ "$mode" = "700" ]; then
    pass "mkdir -m 0700 → mode 700"
else
    fail "mkdir -m 0700 → expected mode 700 got [$mode]"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' mkdir --help

echo "  ── multiple dirs ──"
assert_cmd "" mkdir "$TMPDIR"/mkdir_m1 "$TMPDIR"/mkdir_m2
