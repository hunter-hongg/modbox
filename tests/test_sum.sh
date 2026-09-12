SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sum ─────────────────────────────────────"

TMPF="$TMPDIR/sum_input"
printf 'hello\n' > "$TMPF"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' sum --help

echo "  ── sysv checksum (default) ──"
assert_cmd_pat '^[0-9]+ [0-9]+' sum "$TMPF"

echo "  ── --sysv explicit ──"
assert_cmd_pat '^[0-9]+ [0-9]+' sum --sysv "$TMPF"

echo "  ── --bsd checksum ──"
assert_cmd_pat '^[0-9]+ [0-9]+' sum --bsd "$TMPF"

echo "  ── stdin input ──"
out=$(printf 'hello\n' | "$MODBOX" sum)
if printf '%s' "$out" | grep -qE '^[0-9]+ [0-9]+'; then
    pass "stdin sum produces output"
else
    fail "stdin sum got [$out]"
fi

echo "  ── missing file exits non-zero ──"
if "$MODBOX" sum /nonexistent/xyz >/dev/null 2>&1; then
    fail "sum missing → expected non-zero"
else
    pass "sum missing → exit non-zero"
fi
