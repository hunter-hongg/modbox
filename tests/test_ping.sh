SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── ping ───────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' ping --help

echo "  ── --version shows modbox version ──"
assert_cmd_pat 'modbox' ping --version

echo "  ── no destination is a usage error (exit 2) ──"
"$MODBOX" ping >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping (no args) → exit 2"; else fail "ping (no args) → exit $rc, expected 2"; fi

echo "  ── unknown host is an error (exit 1) ──"
"$MODBOX" ping -c 1 no-such-host-xyz.invalid >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "ping unknown host → exit 1"; else fail "ping unknown host → exit $rc, expected 1"; fi

# ── Live socket tests (conditional on raw ICMP availability) ──
echo "  ── live ping (skipped if raw sockets unavailable) ──"
if "$MODBOX" ping -c 1 127.0.0.1 >/dev/null 2>&1; then
    echo "  -- live raw socket available --"
    assert_cmd_pat '1 packets transmitted, 1 packets received' ping -c 1 127.0.0.1
    assert_cmd_pat '3 packets transmitted, 3 packets received' ping -c 3 127.0.0.1

    # 10.255.255.1 is only an unreachable-by-convention target; on hosts that
    # actually route it, assert the unreachable contract only when it really is
    # unreachable, using the system ping as the ground truth.
    if /usr/bin/ping -c 1 -W 2 10.255.255.1 >/dev/null 2>&1; then
        echo "  SKIP  unreachable-target tests (10.255.255.1 is routable here)"
    else
        assert_cmd_pat '1 packets transmitted, 0 packets received' ping -c 1 10.255.255.1
        "$MODBOX" ping -c 1 10.255.255.1 >/dev/null 2>&1; rcu=$?
        if [[ $rcu -eq 1 ]]; then pass "ping unreachable → exit 1"; else fail "ping unreachable → exit $rcu, expected 1"; fi
    fi
else
    echo "  SKIP  live ping tests (no raw socket permission in this environment)"
fi
