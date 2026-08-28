#!/usr/bin/env bash
#
# test_tcpdump.sh — Test suite for tcpdump command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tcpdump ─────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' tcpdump --help

echo "  ── --version shows modbox version ──"
assert_cmd_pat 'modbox' tcpdump --version

echo "  ── -V shows modbox version ──"
assert_cmd_pat 'modbox' tcpdump -V

echo "  ── tcpdump appears in help listing ──"
assert_cmd_pat 'tcpdump' help

echo "  ── unknown option is a usage error (exit 2) ──"
"$MODBOX" tcpdump --bogus >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "tcpdump --bogus → exit 2"; else fail "tcpdump --bogus → exit $rc, expected 2"; fi

echo "  ── no args prints not-yet-implemented stub (exit 1) ──"
assert_cmd_pat_stderr 'not yet implemented' tcpdump
"$MODBOX" tcpdump >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "tcpdump (no args) → exit 1"; else fail "tcpdump (no args) → exit $rc, expected 1"; fi

echo "  ── -h shows usage ──"
assert_cmd_pat 'Usage:' tcpdump -h

# ── pcap file reading ────────────────────────────────────────────────────────
# Fixture helpers
# LE global header (24B): magic a1b2c3d4, vers 2.0, snaplen ffffffff, linktype 1
pcap_global() {
    local file=$1
    xxd -r -p > "$file" <<'EOF'
a1b2c3d4020000000000000000000000ffffffff01000000
EOF
}

pcap_record() {
    local file=$1 ts_sec=$2 ts_usec=$3 len=$4 data=$5
    xxd -r -p >> "$file" <<EOF
$ts_sec $ts_usec $len 00000000
EOF
    printf '%s' "$data" | xxd -r -p >> "$file"
}

echo "  ── bad magic → error ──"
pcap_global "$TMPDIR/bad.pcap"
printf '\x00\x11\x22\x33' | dd of="$TMPDIR/bad.pcap" bs=1 conv=notrunc 2>/dev/null  # overwrite magic
assert_cmd_pat_stderr 'not a pcap file' tcpdump -r "$TMPDIR/bad.pcap"
"$MODBOX" tcpdump -r "$TMPDIR/bad.pcap" >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "bad magic → exit 1"; else fail "bad magic → exit $rc, expected 1"; fi

echo "  ── missing file → error ──"
assert_cmd_pat_stderr 'cannot open' tcpdump -r "$TMPDIR/nonexistent.pcap"
"$MODBOX" tcpdump -r "$TMPDIR/nonexistent.pcap" >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "missing file → exit 1"; else fail "missing file → exit $rc, expected 1"; fi

echo "  ── unsupported linktype (network=0) → error ──"
xxd -r -p > "$TMPDIR/linktype.pcap" <<'EOF'
a1b2c3d4020000000000000000000000ffffffff00000000
EOF
assert_cmd_pat_stderr 'unsupported linktype' tcpdump -r "$TMPDIR/linktype.pcap"
"$MODBOX" tcpdump -r "$TMPDIR/linktype.pcap" >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "unsupported linktype → exit 1"; else fail "unsupported linktype → exit $rc, expected 1"; fi

echo "  ── header-only file → clean EOF (exit 0, no output) ──"
pcap_global "$TMPDIR/header_only.pcap"
out=$("$MODBOX" tcpdump -r "$TMPDIR/header_only.pcap" 2>/dev/null); rc=$?
if [[ $rc -eq 0 ]]; then pass "header-only → exit 0"; else fail "header-only → exit $rc, expected 0"; fi
if [[ -z "$out" ]]; then pass "header-only → no stdout"; else fail "header-only → unexpected output: [$out]"; fi

echo "  ── big-endian magic file → clean EOF ──"
xxd -r -p > "$TMPDIR/be.pcap" <<'EOF'
d4c3b2a1000200000000000000000000ffffffff00000001
EOF
"$MODBOX" tcpdump -r "$TMPDIR/be.pcap" >/dev/null 2>&1; rc=$?
if [[ $rc -eq 0 ]]; then pass "big-endian → exit 0"; else fail "big-endian → exit $rc, expected 0"; fi

echo "  ── truncated record → error ──"
pcap_global "$TMPDIR/trunc.pcap"
pcap_record "$TMPDIR/trunc.pcap" 00000000 00000000 00000010 0102030405060708
assert_cmd_pat_stderr 'truncated packet record' tcpdump -r "$TMPDIR/trunc.pcap"

# ── decode packet tests ───────────────────────────────────────────────────────
echo "  ── ARP request decode (-tt) ──"
pcap_global "$TMPDIR/arp_req.pcap"
pcap_record "$TMPDIR/arp_req.pcap" 47168a67 40e20100 2a000000 ffffffffffffaabbccddeeff08060001080006040001aabbccddeeffc0a80101000000000000c0a80102
assert_cmd '1737102919.123456 ARP, Request, who has 192.168.1.2 tell 192.168.1.1, length 42' tcpdump -tt -r "$TMPDIR/arp_req.pcap"

echo "  ── ARP request decode with -e ──"
assert_cmd_pat '1737102919.123456 aa:bb:cc:dd:ee:ff > ff:ff:ff:ff:ff:ff, ARP, Request, who has 192.168.1.2 tell 192.168.1.1, length 42' tcpdump -e -tt -r "$TMPDIR/arp_req.pcap"

echo "  ── ARP reply decode ──"
pcap_global "$TMPDIR/arp_reply.pcap"
pcap_record "$TMPDIR/arp_reply.pcap" 47168a67 40e20100 2a000000 ffffffffffffaabbccddeeff08060001080006040002aabbccddeeffc0a80101000000000000c0a80102
assert_cmd '1737102919.123456 ARP, Reply, 192.168.1.1 is-at aa:bb:cc:dd:ee:ff, length 42' tcpdump -tt -r "$TMPDIR/arp_reply.pcap"

echo "  ── IPv4 generic proto 47 ──"
pcap_global "$TMPDIR/ipv4.pcap"
pcap_record "$TMPDIR/ipv4.pcap" 47168a67 40e20100 36000000 ffffffffffffaabbccddeeff08004500140000000000402f0000c0a80102c0a801010000000000000000000000000000000000000000
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: IP, proto 47, length 20' tcpdump -tt -r "$TMPDIR/ipv4.pcap"

echo "  ── unknown EtherType ──"
pcap_global "$TMPDIR/unknown.pcap"
pcap_record "$TMPDIR/unknown.pcap" 47168a67 40e20100 0e000000 ffffffffffffaabbccddeeff9999
assert_cmd '1737102919.123456 EtherType 0x9999, length 14' tcpdump -tt -r "$TMPDIR/unknown.pcap"

echo "  ── default timestamp format ──"
pcap_global "$TMPDIR/ts.pcap"
pcap_record "$TMPDIR/ts.pcap" 47168a67 40e20100 2a000000 ffffffffffffaabbccddeeff08060001080006040001aabbccddeeffc0a80101000000000000c0a80102
assert_cmd_pat '^[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{6} ' tcpdump -r "$TMPDIR/ts.pcap"

echo "  ── short record skipped silently ──"
pcap_global "$TMPDIR/short.pcap"
pcap_record "$TMPDIR/short.pcap" 47168a67 40e20100 0a000000 0102030405060708090a
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/short.pcap" 2>/dev/null); rc=$?
if [[ $rc -eq 0 && -z "$out" ]]; then pass "short record → no output"; else fail "short record → rc=$rc out=[$out]"; fi

echo "  ── IPv6/UDP decode (-tt) ──"
pcap_global "$TMPDIR/ipv6_udp.pcap"
pcap_record "$TMPDIR/ipv6_udp.pcap" 47168a67 40e20100 4a000000 ffffffffffffaabbccddeeff86dd600000000014114020010db800000000000000000000000220010db8000000000000000000000001829a003500140000000000000000000000000000
assert_cmd '1737102919.123456 2001:db8::2.33434 > 2001:db8::1.53: UDP, length 12' tcpdump -tt -r "$TMPDIR/ipv6_udp.pcap"

echo "  ── IPv6/UDP decode with -v (hop limit) ──"
assert_cmd_pat '1737102919.123456 2001:db8::2.33434 > 2001:db8::1.53: UDP, length 12, hop limit 64' tcpdump -v -tt -r "$TMPDIR/ipv6_udp.pcap"

echo "  ── IPv6/TCP SYN decode (-tt) ──"
pcap_global "$TMPDIR/ipv6_tcp.pcap"
pcap_record "$TMPDIR/ipv6_tcp.pcap" 47168a67 40e20100 4a000000 ffffffffffffaabbccddeeff86dd600000000014064020010db800000000000000000000000220010db80000000000000000000000019c40005000000007000000005002ffff00000000
assert_cmd '1737102919.123456 2001:db8::2.40000 > 2001:db8::1.80: Flags [S], seq 7, win 65535, length 0' tcpdump -tt -r "$TMPDIR/ipv6_tcp.pcap"

echo "  ── IPv6/ICMPv6 echo request decode (-tt) ──"
pcap_global "$TMPDIR/ipv6_icmp.pcap"
pcap_record "$TMPDIR/ipv6_icmp.pcap" 47168a67 40e20100 42000000 ffffffffffffaabbccddeeff86dd6000000000143a4020010db800000000000000000000000220010db8000000000000000000000001800000000001000100000000
assert_cmd '1737102919.123456 2001:db8::2 > 2001:db8::1: ICMP6, echo request, id 1, seq 1' tcpdump -tt -r "$TMPDIR/ipv6_icmp.pcap"

echo "  ── IPv6 generic next header decode (-tt) ──"
pcap_global "$TMPDIR/ipv6_generic.pcap"
pcap_record "$TMPDIR/ipv6_generic.pcap" 47168a67 40e20100 36000000 ffffffffffffaabbccddeeff86dd6000000000002b4020010db800000000000000000000000220010db8000000000000000000000001
assert_cmd '1737102919.123456 2001:db8::2 > 2001:db8::1: IP6, next 43, length 0' tcpdump -tt -r "$TMPDIR/ipv6_generic.pcap"

echo "  ── truncated IPv6 record skipped silently ──"
pcap_global "$TMPDIR/ipv6_short.pcap"
pcap_record "$TMPDIR/ipv6_short.pcap" 47168a67 40e20100 30000000 fffffffffffaabbccddeeff86dd000000000000000000000000000000000000000000000000000000000000000000000
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/ipv6_short.pcap" 2>/dev/null); rc=$?
if [[ $rc -eq 0 && -z "$out" ]]; then pass "truncated IPv6 → no output"; else fail "truncated IPv6 → rc=$rc out=[$out]"; fi
