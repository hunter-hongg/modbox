SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sleep ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' sleep --help

echo "  ── --version ──"
assert_cmd_pat 'sleep' sleep --version

echo "  ── sleeps briefly ──"
start=$(date +%s%N)
"$MODBOX" sleep 0.1 </dev/null
end=$(date +%s%N)
elapsed_ms=$(( (end - start) / 1000000 ))
if [[ $elapsed_ms -ge 90 ]]; then
    pass "sleep 0.1 slept ${elapsed_ms}ms"
else
    fail "sleep 0.1 only slept ${elapsed_ms}ms"
fi

echo "  ── suffix s ──"
"$MODBOX" sleep 0s </dev/null 2>/dev/null && pass "sleep 0s" || fail "sleep 0s"

echo "  ── invalid arg errors ──"
if "$MODBOX" sleep notanumber </dev/null >/dev/null 2>&1; then
    fail "sleep notanumber → expected non-zero"
else
    pass "sleep notanumber → exit non-zero"
fi
