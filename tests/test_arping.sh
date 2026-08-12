SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── arping ───────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' arping --help

echo "  ── --version shows modbox version ──"
assert_cmd_pat 'modbox' arping --version

echo "  ── no target IP is a usage error (exit 2) ──"
"$MODBOX" arping >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "arping (no args) → exit 2"; else fail "arping (no args) → exit $rc, expected 2"; fi

echo "  ── missing -I interface is an error (exit 2) ──"
"$MODBOX" arping 192.168.1.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "arping without -I → exit 2"; else fail "arping without -I → exit $rc, expected 2"; fi

echo "  ── nonexistent interface is an error (exit 1) ──"
"$MODBOX" arping -I no-such-iface-xyz 192.168.1.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "arping bad iface → exit 1"; else fail "arping bad iface → exit $rc, expected 1"; fi

# ── Discoverability ──
echo "  ── arping appears in help listing ──"
assert_cmd_pat 'arping' help
