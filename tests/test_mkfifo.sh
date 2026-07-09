SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── mkfifo ────────────────────────────────────"

echo "  ── basic create ──"
assert_cmd "" mkfifo "$TMPDIR"/mkfifo_new
test -p "$TMPDIR"/mkfifo_new && pass "mkfifo_new is a FIFO" || fail "mkfifo_new not created"

echo "  ── already exists ──"
assert_cmd_pat_stderr "File exists" mkfifo "$TMPDIR"/mkfifo_new

echo "  ── mode ──"
assert_cmd "" mkfifo -m 0640 "$TMPDIR"/mkfifo_mode
mode=$(stat -c '%a' "$TMPDIR"/mkfifo_mode 2>/dev/null)
if [ "$mode" = "640" ]; then
    pass "mkfifo -m 0640 → mode 640"
else
    fail "mkfifo -m 0640 → expected mode 640 got [$mode]"
fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' mkfifo --help

echo "  ── multiple fifos ──"
assert_cmd "" mkfifo "$TMPDIR"/mkfifo_a "$TMPDIR"/mkfifo_b
test -p "$TMPDIR"/mkfifo_a && pass "mkfifo_a exists" || fail "mkfifo_a not created"
test -p "$TMPDIR"/mkfifo_b && pass "mkfifo_b exists" || fail "mkfifo_b not created"
