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

# ── Option validation (#57): usage errors, no raw socket needed ──
echo "  ── invalid -w values are rejected (exit 2) ──"
for bad in x -1 1x; do
"$MODBOX" arping -w "$bad" -I lo 192.168.1.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "arping -w $bad → exit 2"; else fail "arping -w $bad → exit $rc, expected 2"; fi
done

echo "  ── -w 0.2 is accepted (fractional seconds) ──"
"$MODBOX" arping -w 0.2 -I lo 192.168.1.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then fail "arping -w 0.2 → exit 2, expected acceptance"; else pass "arping -w 0.2 → exit $rc (not a usage error)"; fi

echo "  ── invalid target IP is an error (exit 1) ──"
"$MODBOX" arping -I lo not-an-ip >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "arping bad IP → exit 1"; else fail "arping bad IP → exit $rc, expected 1"; fi

echo "  ── -D/-q compose with -c/-I and an IP (no usage error) ──"
"$MODBOX" arping -D -q -c 1 -I lo 192.168.1.1 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then fail "arping -D -q -c 1 -I → exit 2, expected composition"; else pass "arping -D -q -c 1 -I lo → exit $rc (not a usage error)"; fi

# ── Live tests (require CAP_NET_RAW; behaviour is host-dependent) ──
if "$MODBOX" arping -c 1 -I lo 127.0.0.1 2>&1 | grep -q 'raw packet socket'; then
    echo "  SKIP  live arping tests (no CAP_NET_RAW in this environment)"
else
    echo "  -- raw socket available; live ARP behaviour is host-dependent --"
    # -w must bound the wait: a probe for an address nothing answers for
    # returns in roughly the timeout, not the 1 s default.
    iface=$(ip -o -4 route show default 2>/dev/null | awk '{for(i=1;i<=NF;i++) if($i=="dev") print $(i+1); exit}')
    if [[ -n "$iface" ]]; then
        start=$(date +%s%N)
        "$MODBOX" arping -w 0.2 -c 1 -I "$iface" 192.0.2.77 >/dev/null 2>&1
        end=$(date +%s%N)
        elapsed_ms=$(( (end - start) / 1000000 ))
        if [[ $elapsed_ms -lt 1500 ]]; then pass "arping -w 0.2 returned in ${elapsed_ms}ms (< 1500ms)"; else fail "arping -w 0.2 took ${elapsed_ms}ms, expected < 1500ms"; fi
    fi
fi

# ── Namespaced ARP loop (full behaviour: resolve, -q, -D, -w) ──
# A veth pair split across two network namespaces gives a peer whose kernel
# answers ARP for real. Requires unshare/nsenter; skipped when unavailable.
run_arp_ns_tests() {
    local script="$TMPDIR/arp_ns_peer.sh"
    cat > "$script" <<'NS'
set -e
MODBOX_BIN="$1"
PEERFILE="$2"
ip link set lo up 2>/dev/null
unshare -n bash -c "sleep 15 & echo \$! > $PEERFILE; wait" &
sleep 0.4
PEER=$(cat "$PEERFILE" 2>/dev/null) || exit 1
ip link add arp0 type veth peer name arp1
ip link set arp1 netns $PEER
ip link set arp0 up; ip addr add 10.98.0.1/24 dev arp0
nsenter -t $PEER -n ip link set lo up
nsenter -t $PEER -n ip link set arp1 up
nsenter -t $PEER -n ip addr add 10.98.0.2/24 dev arp1
out=$("$MODBOX_BIN" arping -c1 -I arp0 10.98.0.2 2>&1) && rc=0 || rc=$?; echo "R1 $rc $out"
out=$("$MODBOX_BIN" arping -q -c1 -I arp0 10.98.0.2 2>&1) && rc=0 || rc=$?; echo "R2 $rc $out"
out=$("$MODBOX_BIN" arping -D -c1 -I arp0 10.98.0.2 2>&1) && rc=0 || rc=$?; echo "R3 $rc $out"
out=$("$MODBOX_BIN" arping -D -c1 -w 0.3 -I arp0 10.98.0.77 2>&1) && rc=0 || rc=$?; echo "R4 $rc $out"
start=$(date +%s%N)
"$MODBOX_BIN" arping -c1 -w 0.3 -I arp0 10.98.0.78 >/dev/null 2>&1 || true
end=$(date +%s%N)
echo "R5 ms $(( (end - start) / 1000000 ))"
kill $PEER 2>/dev/null || true
NS
    unshare -rn bash "$script" "$MODBOX" "$TMPDIR/modbox_arp_peer" 2>/dev/null
}

arp_ns_out=$(run_arp_ns_tests || true)
if ! printf '%s' "$arp_ns_out" | grep -q '^R1 '; then
    echo "  SKIP  namespace ARP tests (unshare/nsenter/veth unavailable)"
else
    if printf '%s\n' "$arp_ns_out" | grep -qE '^R1 0 ARP REPLY 10\.98\.0\.2: ([0-9a-f]{2}:){5}[0-9a-f]{2}$'; then
        pass "arping resolves a real ARP peer"
    else
        fail "arping resolve in namespace: [$(printf '%s\n' "$arp_ns_out" | grep '^R1 ')]"
    fi
    if printf '%s\n' "$arp_ns_out" | grep -qE '^R2 0 ([0-9a-f]{2}:){5}[0-9a-f]{2}$'; then
        pass "arping -q prints only the MAC"
    else
        fail "arping -q: [$(printf '%s\n' "$arp_ns_out" | grep '^R2 ')]"
    fi
    if printf '%s\n' "$arp_ns_out" | grep -qE '^R3 1 10\.98\.0\.2 is already in use'; then
        pass "arping -D detects a conflicting peer (exit 1)"
    else
        fail "arping -D conflict: [$(printf '%s\n' "$arp_ns_out" | grep '^R3 ')]"
    fi
    if printf '%s\n' "$arp_ns_out" | grep -qE '^R4 0 10\.98\.0\.77 is free'; then
        pass "arping -D reports an unused address free (exit 0)"
    else
        fail "arping -D free: [$(printf '%s\n' "$arp_ns_out" | grep '^R4 ')]"
    fi
    r5_ms=$(printf '%s\n' "$arp_ns_out" | awk '/^R5 ms/{print $3}')
    if [[ -n "$r5_ms" && "$r5_ms" -lt 1500 ]]; then
        pass "arping -w 0.3 bounded the wait (${r5_ms}ms < 1500ms)"
    else
        fail "arping -w timing: ${r5_ms}ms, expected < 1500ms"
    fi
fi

# ── Discoverability ──
echo "  ── arping appears in help listing ──"
assert_cmd_pat 'arping' help
