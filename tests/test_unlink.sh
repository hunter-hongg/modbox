#!/usr/bin/env bash
#
# test_unlink.sh — Test the unlink command
#

# Source the test framework
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── unlink ────────────────────────────────────"

echo "test content" > "$TMPDIR"/unlink_test.txt

echo "  ── basic unlink ──"
"$MODBOX" unlink "$TMPDIR"/unlink_test.txt
if [[ ! -f "$TMPDIR"/unlink_test.txt ]]; then
    pass "unlink → file removed"
else
    fail "unlink — file still exists"
fi

echo "  ── -v : verbose ──"
echo "test content" > "$TMPDIR"/unlink_v.txt
out=$("$MODBOX" unlink -v "$TMPDIR"/unlink_v.txt 2>/dev/null || true)
if echo "$out" | grep -qE "unlinked '.*unlink_v.*'"; then
    pass "unlink -v → verbose output"
else
    fail "unlink -v — expected verbose output, got [$out]"
fi

echo "  ── error: non-existent file ──"
assert_cmd_pat_stderr "cannot unlink" unlink "$TMPDIR"/nonexistent

echo "  ── error: missing argument ──"
assert_cmd_pat_stderr "missing option FILE" unlink

echo "  ── help output ──"
out=$("$MODBOX" unlink -h 2>/dev/null || true)
if echo "$out" | grep -q "Usage:"; then
    pass "unlink -h → help output"
else
    fail "unlink -h — expected help output, got [$out]"
fi
