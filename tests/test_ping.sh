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

# ── Option validation (#55/#56): all usage errors, no sockets needed ──
echo "  ── -s 0 is rejected (exit 2) ──"
"$MODBOX" ping -s 0 -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -s 0 → exit 2"; else fail "ping -s 0 → exit $rc, expected 2"; fi

echo "  ── -s 99999 out of range (exit 2) ──"
"$MODBOX" ping -s 99999 -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -s 99999 → exit 2"; else fail "ping -s 99999 → exit $rc, expected 2"; fi

echo "  ── -s garbage is rejected (exit 2) ──"
"$MODBOX" ping -s 12x -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -s 12x → exit 2"; else fail "ping -s 12x → exit $rc, expected 2"; fi

echo "  ── invalid -i values are rejected (exit 2) ──"
for bad in abc -1 1x; do
"$MODBOX" ping -i "$bad" -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -i $bad → exit 2"; else fail "ping -i $bad → exit $rc, expected 2"; fi
done

echo "  ── invalid -W value is rejected (exit 2) ──"
for bad in x -0.5; do
"$MODBOX" ping -W "$bad" -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -W $bad → exit 2"; else fail "ping -W $bad → exit $rc, expected 2"; fi
done

echo "  ── -4 -6 together is a usage error (exit 2) ──"
"$MODBOX" ping -4 -6 -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -4 -6 → exit 2"; else fail "ping -4 -6 → exit $rc, expected 2"; fi

echo "  ── family mismatch is reported (exit 2, not silent fallback) ──"
"$MODBOX" ping -4 -c 1 ::1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -4 <v6-host> → exit 2"; else fail "ping -4 <v6-host> → exit $rc, expected 2"; fi
"$MODBOX" ping -6 -c 1 127.0.0.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "ping -6 <v4-host> → exit 2"; else fail "ping -6 <v4-host> → exit $rc, expected 2"; fi

echo "  ── -4/-6 --help still show usage and exit 0 ──"
assert_cmd_pat 'Usage:' ping -4 --help
assert_cmd_pat 'Usage:' ping -6 --help

# ── Live socket tests (conditional on raw ICMP availability) ──
echo "  ── live ping (skipped if raw sockets unavailable) ──"
if "$MODBOX" ping -c 1 127.0.0.1 >/dev/null 2>&1; then
    echo "  -- live raw socket available --"
    assert_cmd_pat '1 packets transmitted, 1 packets received' ping -c 1 127.0.0.1
    assert_cmd_pat '3 packets transmitted, 3 packets received' ping -c 3 127.0.0.1

    # Tunables (#55).
    assert_cmd_pat '100\(128\) bytes of data\.' ping -c 1 -s 100 127.0.0.1
    assert_cmd_pat 'icmp_seq=1 ttl=[0-9]+ time=' ping -c 1 127.0.0.1
    assert_cmd_pat 'PING .* bytes of data\.' ping -q -c 1 127.0.0.1
    assert_cmd_not_pat 'bytes from' ping -q -c 1 127.0.0.1
    assert_cmd_pat '1 packets transmitted, 1 packets received' ping -q -c 1 127.0.0.1
    # -i 0.5 x 3 probes must finish well under 3 single-second intervals.
    start=$(date +%s%N)
    "$MODBOX" ping -q -i 0.5 -c 3 127.0.0.1 >/dev/null 2>&1
    end=$(date +%s%N)
    elapsed_ms=$(( (end - start) / 1000000 ))
    if [[ $elapsed_ms -lt 2000 ]]; then pass "ping -i 0.5 -c 3 finished in ${elapsed_ms}ms (< 2000ms)"; else fail "ping -i 0.5 -c 3 took ${elapsed_ms}ms, expected < 2000ms"; fi

    # -W must bound the per-probe wait: a blackhole host with -W 1 returns
    # within ~1s (plus slack) and reports loss.
    if ! /usr/bin/ping -c 1 -W 1 192.0.2.1 >/dev/null 2>&1; then
        start=$(date +%s%N)
        "$MODBOX" ping -W 1 -c 1 192.0.2.1 >/dev/null 2>&1
        end=$(date +%s%N)
        elapsed_ms=$(( (end - start) / 1000000 ))
        if [[ $elapsed_ms -lt 3000 ]]; then pass "ping -W 1 blackhole returned in ${elapsed_ms}ms (< 3000ms)"; else fail "ping -W 1 blackhole took ${elapsed_ms}ms, expected < 3000ms"; fi
        assert_cmd_pat '1 packets transmitted, 0 packets received, 100% packet loss' ping -W 1 -c 1 192.0.2.1
    fi

    # IPv6 (#56) — only exercised when an IPv6 ping socket is available.
    if "$MODBOX" ping -6 -c 1 ::1 >/dev/null 2>&1; then
        echo "  -- live IPv6 ping available --"
        assert_cmd_pat '1 packets transmitted, 1 packets received' ping -6 -c 1 ::1
        assert_cmd_pat 'PING ::1 \(::1\)' ping -6 -c 1 ::1
        assert_cmd_pat 'rtt min/avg/max/mdev = ' ping -6 -c 1 ::1
        assert_cmd_pat '1 packets transmitted, 1 packets received' ping -6 -q -c 1 ::1
        assert_cmd_not_pat 'bytes from' ping -6 -q -c 1 ::1
        # auto family selection: no -6 flag needed for a v6 literal
        assert_cmd_pat '1 packets transmitted, 1 packets received' ping -c 1 ::1
    else
        echo "  SKIP  IPv6 live tests (no IPv6 ping socket in this environment)"
    fi

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
