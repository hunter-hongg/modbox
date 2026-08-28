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

# Canonical 6-record corpus (5 decodable + 1 garbage), reused by filter/writer tests
# 1 ARP request, 2 TCP SYN, 3 UDP, 4 ICMP echo, 5 IPv6/UDP, 6 10-byte garbage
write_corpus() {
    local file=$1
    pcap_global "$file"
    pcap_record "$file" 47168a67 40e20100 2a000000 ffffffffffffaabbccddeeff08060001080006040001aabbccddeeffc0a80101000000000000c0a80102
    pcap_record "$file" 47168a67 40e20100 36000000 ffffffffffffaabbccddeeff08004500002810e1400040060000c0a80102c0a80101c82201bb0001e240000000005002faf000000000
    pcap_record "$file" 47168a67 40e20100 54000000 ffffffffffffaabbccddeeff08004500004610e1004040110000c0a80103c0a80101cfdb003500320000787878787878787878787878787878787878787878787878787878787878787878787878787878787878
    pcap_record "$file" 47168a67 40e20100 2e000000 ffffffffffffaabbccddeeff08004500002010e1004040010000c0a80104c0a80101080000000001000161626364
    pcap_record "$file" 47168a67 40e20100 4a000000 ffffffffffffaabbccddeeff86dd600000000014114020010db800000000000000000000000220010db8000000000000000000000001829a0035001400007a7a7a7a7a7a7a7a7a7a7a7a
    pcap_record "$file" 47168a67 40e20100 0a000000 0102030405060708090a
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

echo "  ── IPv4/TCP SYN decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_tcp_syn.pcap"
pcap_record "$TMPDIR/ipv4_tcp_syn.pcap" 47168a67 40e20100 36000000 ffffffffffffaabbccddeeff08004500002810e1400040060000c0a80102c0a80101c82201bb0001e240000000005002faf000000000
assert_cmd '1737102919.123456 192.168.1.2.51234 > 192.168.1.1.443: Flags [S], seq 123456, win 64240, length 0' tcpdump -tt -r "$TMPDIR/ipv4_tcp_syn.pcap"

echo "  ── IPv4/TCP PSH+ACK decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_tcp_psh.pcap"
pcap_record "$TMPDIR/ipv4_tcp_psh.pcap" 47168a67 40e20100 3a000000 ffffffffffffaabbccddeeff08004500002c10e1400040060000c0a80102c0a80101c82201bb0001e2400001e2415018faf00000000061626364
assert_cmd '1737102919.123456 192.168.1.2.51234 > 192.168.1.1.443: Flags [P.], seq 123456, ack 123457, win 64240, length 4' tcpdump -tt -r "$TMPDIR/ipv4_tcp_psh.pcap"

echo "  ── IPv4/TCP FIN decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_tcp_fin.pcap"
pcap_record "$TMPDIR/ipv4_tcp_fin.pcap" 47168a67 40e20100 36000000 ffffffffffffaabbccddeeff08004500002810e1400040060000c0a80102c0a80101c82201bb0001e2400001e2415011faf000000000
assert_cmd '1737102919.123456 192.168.1.2.51234 > 192.168.1.1.443: Flags [F.], seq 123456, ack 123457, win 64240, length 0' tcpdump -tt -r "$TMPDIR/ipv4_tcp_fin.pcap"

echo "  ── IPv4/TCP bare ACK decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_tcp_ack.pcap"
pcap_record "$TMPDIR/ipv4_tcp_ack.pcap" 47168a67 40e20100 36000000 ffffffffffffaabbccddeeff08004500002810e1400040060000c0a80102c0a80101c82201bb0001e2400001e2415010faf000000000
assert_cmd '1737102919.123456 192.168.1.2.51234 > 192.168.1.1.443: Flags [.], seq 123456, ack 123457, win 64240, length 0' tcpdump -tt -r "$TMPDIR/ipv4_tcp_ack.pcap"

echo "  ── IPv4/TCP -v shows ack, ttl, id, DF ──"
assert_cmd_pat '1737102919.123456 192.168.1.2.51234 > 192.168.1.1.443: Flags \[S\], seq 123456, ack 0, win 64240, length 0, ttl 64, id 4321, DF' tcpdump -v -tt -r "$TMPDIR/ipv4_tcp_syn.pcap"

echo "  ── IPv4/TCP -v decodes options ──"
pcap_global "$TMPDIR/ipv4_tcp_opts.pcap"
pcap_record "$TMPDIR/ipv4_tcp_opts.pcap" 47168a67 40e20100 4a000000 ffffffffffffaabbccddeeff08004500003c10e1400040060000c0a80102c0a80101c82201bb0001e24000000000a002faf000000000020405b40402080a000000640000003201030307
assert_cmd '1737102919.123456 192.168.1.2.51234 > 192.168.1.1.443: Flags [S], seq 123456, ack 0, win 64240, length 0, options [mss 1460, sackOK, TS val 100 ecr 50, nop, wscale 7], ttl 64, id 4321, DF' tcpdump -v -tt -r "$TMPDIR/ipv4_tcp_opts.pcap"

echo "  ── short TCP record skipped silently ──"
pcap_global "$TMPDIR/ipv4_tcp_short.pcap"
pcap_record "$TMPDIR/ipv4_tcp_short.pcap" 47168a67 40e20100 28000000 ffffffffffffaabbccddeeff08004500002810e1400040060000c0a80102c0a80101c82201bb0001
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/ipv4_tcp_short.pcap" 2>/dev/null); rc=$?
if [[ $rc -eq 0 && -z "$out" ]]; then pass "short TCP → no output"; else fail "short TCP → rc=$rc out=[$out]"; fi

echo "  ── IPv4/UDP decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_udp.pcap"
pcap_record "$TMPDIR/ipv4_udp.pcap" 47168a67 40e20100 54000000 ffffffffffffaabbccddeeff08004500004610e1004040110000c0a80102c0a80101cfdb003500320000787878787878787878787878787878787878787878787878787878787878787878787878787878787878
assert_cmd '1737102919.123456 192.168.1.2.53211 > 192.168.1.1.53: UDP, length 42' tcpdump -tt -r "$TMPDIR/ipv4_udp.pcap"

echo "  ── IPv4/UDP declared length clamped to available bytes ──"
pcap_global "$TMPDIR/ipv4_udp_clamp.pcap"
pcap_record "$TMPDIR/ipv4_udp_clamp.pcap" 47168a67 40e20100 4c000000 ffffffffffffaabbccddeeff08004500003410e1004040110000c0a80102c0a80101cfdb003500c8000078787878787878787878787878787878787878787878787878787878787878787878
assert_cmd '1737102919.123456 192.168.1.2.53211 > 192.168.1.1.53: UDP, length 34' tcpdump -tt -r "$TMPDIR/ipv4_udp_clamp.pcap"

echo "  ── IPv4/ICMP echo request decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_icmp_req.pcap"
pcap_record "$TMPDIR/ipv4_icmp_req.pcap" 47168a67 40e20100 2e000000 ffffffffffffaabbccddeeff08004500002010e1004040010000c0a80102c0a80101080000000001000161626364
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: ICMP echo request, id 1, seq 1, length 12' tcpdump -tt -r "$TMPDIR/ipv4_icmp_req.pcap"

echo "  ── IPv4/ICMP echo reply decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_icmp_rep.pcap"
pcap_record "$TMPDIR/ipv4_icmp_rep.pcap" 47168a67 40e20100 2e000000 ffffffffffffaabbccddeeff08004500002010e1004040010000c0a80102c0a80101000000000001000161626364
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: ICMP echo reply, id 1, seq 1, length 12' tcpdump -tt -r "$TMPDIR/ipv4_icmp_rep.pcap"

echo "  ── IPv4/ICMP dest unreachable decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_icmp_unreach.pcap"
pcap_record "$TMPDIR/ipv4_icmp_unreach.pcap" 47168a67 40e20100 2e000000 ffffffffffffaabbccddeeff08004500002010e1004040010000c0a80102c0a80101030400000000000000000000
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: ICMP destination unreachable, length 12' tcpdump -tt -r "$TMPDIR/ipv4_icmp_unreach.pcap"

echo "  ── IPv4/ICMP time exceeded decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_icmp_ttl.pcap"
pcap_record "$TMPDIR/ipv4_icmp_ttl.pcap" 47168a67 40e20100 2e000000 ffffffffffffaabbccddeeff08004500002010e1004040010000c0a80102c0a801010b0000000000000000000000
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: ICMP time exceeded, length 12' tcpdump -tt -r "$TMPDIR/ipv4_icmp_ttl.pcap"

echo "  ── IPv4/ICMP unknown type decode (-tt) ──"
pcap_global "$TMPDIR/ipv4_icmp_t4.pcap"
pcap_record "$TMPDIR/ipv4_icmp_t4.pcap" 47168a67 40e20100 2e000000 ffffffffffffaabbccddeeff08004500002010e1004040010000c0a80102c0a80101040000000000000000000000
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: ICMP type 4 code 0, length 12' tcpdump -tt -r "$TMPDIR/ipv4_icmp_t4.pcap"

# ── canonical corpus (5 decodable records + 1 garbage) ─────────────────────
echo "  ── corpus decodes to exactly 5 lines ──"
write_corpus "$TMPDIR/corpus.pcap"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" 2>/dev/null); rc=$?
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $rc -eq 0 && $lines -eq 5 ]]; then pass "corpus → 5 lines"; else fail "corpus → rc=$rc lines=$lines out=[$out]"; fi

echo "  ── corpus line contracts ──"
assert_cmd_pat '1737102919.123456 ARP, Request, who has 192\.168\.1\.2 tell 192\.168\.1\.1, length 42' tcpdump -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '192\.168\.1\.2\.51234 > 192\.168\.1\.1\.443: Flags \[S\], seq 123456, win 64240, length 0' tcpdump -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '192\.168\.1\.3\.53211 > 192\.168\.1\.1\.53: UDP, length 42' tcpdump -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '192\.168\.1\.4 > 192\.168\.1\.1: ICMP echo request, id 1, seq 1, length 12' tcpdump -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '2001:db8::2\.33434 > 2001:db8::1\.53: UDP, length 12' tcpdump -tt -r "$TMPDIR/corpus.pcap"

echo "  ── garbage record alone produces no line ──"
pcap_global "$TMPDIR/garbage.pcap"
pcap_record "$TMPDIR/garbage.pcap" 47168a67 40e20100 0a000000 0102030405060708090a
assert_cmd_not_pat 'Flags|UDP,|ICMP|ARP,' tcpdump -tt -r "$TMPDIR/garbage.pcap"

# ── filter expression parser ───────────────────────────────────────────────
echo "  ── filter parse errors → exit 2 with filter error ──"
for expr in '' 'frobnicate 1' 'host' '(tcp' 'tcp)' 'tcp and'; do
    "$MODBOX" tcpdump -r "$TMPDIR/corpus.pcap" -f "$expr" >/dev/null 2>"$TMPDIR/ferr"; rc=$?
    if [[ $rc -eq 2 ]] && grep -q "filter error" "$TMPDIR/ferr"; then
        pass "-f '$expr' → exit 2 + filter error"
    else
        fail "-f '$expr' → rc=$rc stderr=[$(cat "$TMPDIR/ferr")]"
    fi
done

echo "  ── valid filter parses and evaluates ──"
b=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f '(tcp or udp) and not icmp' 2>/dev/null); rc=$?
lines=$(printf '%s\n' "$b" | grep -c .)
if [[ $rc -eq 0 && $lines -eq 3 ]]; then pass "filter '(tcp or udp) and not icmp' → 3 lines"; else fail "→ rc=$rc lines=$lines"; fi

echo "  ── -f 'port 99999' → parse error (range) ──"
"$MODBOX" tcpdump -r "$TMPDIR/corpus.pcap" -f 'port 99999' >/dev/null 2>"$TMPDIR/ferr"; rc=$?
if [[ $rc -eq 2 ]] && grep -q "filter error" "$TMPDIR/ferr"; then
    pass "-f 'port 99999' → exit 2 + filter error"
else
    fail "-f 'port 99999' → rc=$rc stderr=[$(cat "$TMPDIR/ferr")]"
fi

# ── filter atoms + process_packet pipeline + -c ──────────────────────────
echo "  ── filter: host 192.168.1.1 (4 lines) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'host 192.168.1.1' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 4 ]]; then pass "-f 'host 192.168.1.1' → 4 lines"; else fail "-f 'host 192.168.1.1' → $lines lines"; fi

echo "  ── filter: host 2001:db8::1 (1 line) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'host 2001:db8::1' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 1 ]]; then pass "-f 'host 2001:db8::1' → 1 line"; else fail "-f 'host 2001:db8::1' → $lines lines"; fi

echo "  ── filter: port 53 (2 lines: IPv4 UDP + IPv6 UDP) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'port 53' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 2 ]]; then pass "-f 'port 53' → 2 lines"; else fail "-f 'port 53' → $lines lines"; fi

echo "  ── filter: tcp (1 line) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'tcp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 1 ]]; then pass "-f 'tcp' → 1 line"; else fail "-f 'tcp' → $lines lines"; fi

echo "  ── filter: proto udp (2 lines: IPv4 UDP + IPv6 UDP) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'proto udp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 2 ]]; then pass "-f 'proto udp' → 2 lines"; else fail "-f 'proto udp' → $lines lines"; fi

echo "  ── filter: not arp (4 lines) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'not arp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 4 ]]; then pass "-f 'not arp' → 4 lines"; else fail "-f 'not arp' → $lines lines"; fi

echo "  ── filter: arp and host 192.168.1.1 (1 line) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'arp and host 192.168.1.1' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 1 ]]; then pass "-f 'arp and host 192.168.1.1' → 1 line"; else fail "-f 'arp and host 192.168.1.1' → $lines lines (got: [$out])"; fi

echo "  ── filter: src 192.168.1.2 (1 line) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'src 192.168.1.2' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 1 ]]; then pass "-f 'src 192.168.1.2' → 1 line"; else fail "-f 'src 192.168.1.2' → $lines lines"; fi

echo "  ── filter: dst 53 (2 lines: IPv4 UDP + IPv6 UDP) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'dst 53' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 2 ]]; then pass "-f 'dst 53' → 2 lines"; else fail "-f 'dst 53' → $lines lines"; fi

echo "  ── filter: net 192.168.1.0/24 (4 lines) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'net 192.168.1.0/24' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 4 ]]; then pass "-f 'net 192.168.1.0/24' → 4 lines"; else fail "-f 'net 192.168.1.0/24' → $lines lines"; fi

echo "  ── filter: net 192.168.1.0 (0 lines, /32) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'net 192.168.1.0' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 0 ]]; then pass "-f 'net 192.168.1.0' → 0 lines"; else fail "-f 'net 192.168.1.0' → $lines lines"; fi

echo "  ── filter: ip (3 lines: tcp, udp, icmp) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'ip' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 3 ]]; then pass "-f 'ip' → 3 lines"; else fail "-f 'ip' → $lines lines"; fi

echo "  ── filter: icmp (1 line) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'icmp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 1 ]]; then pass "-f 'icmp' → 1 line"; else fail "-f 'icmp' → $lines lines"; fi

echo "  ── filter: (tcp or udp) and not arp (3 lines: TCP + 2 UDPs) ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f '(tcp or udp) and not arp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 3 ]]; then pass "-f '(tcp or udp) and not arp' → 3 lines"; else fail "-f '(tcp or udp) and not arp' → $lines lines"; fi

echo "  ── -c 2 → exactly 2 lines ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -c 2 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 2 ]]; then pass "-c 2 → 2 lines"; else fail "-c 2 → $lines lines"; fi

echo "  ── -c 0 = unlimited, -c 0 -f 'udp' → 2 lines ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -c 0 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 5 ]]; then pass "-c 0 → 5 lines"; else fail "-c 0 → $lines lines"; fi
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -c 0 -f 'udp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 2 ]]; then pass "-c 0 -f 'udp' → 2 lines"; else fail "-c 0 -f 'udp' → $lines lines"; fi

echo "  ── undecodable record dropped by every filter ──"
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" -f 'not arp' 2>/dev/null)
lines=$(printf '%s\n' "$out" | grep -c .)
if [[ $lines -eq 4 ]]; then pass "garbage never printed under filter → 4 lines"; else fail "→ $lines lines"; fi

# ── display options: -q -e -x -s -tt -n/-nn ───────────────────────────────
echo "  ── -q brief output ──"
assert_cmd_pat '192\.168\.1\.2 > 192\.168\.1\.1: TCP, length 0' tcpdump -q -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '192\.168\.1\.3 > 192\.168\.1\.1: UDP, length 42' tcpdump -q -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '192\.168\.1\.4 > 192\.168\.1\.1: ICMP, length 12' tcpdump -q -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat 'ARP, length 42' tcpdump -q -tt -r "$TMPDIR/corpus.pcap"
assert_cmd_pat '2001:db8::2 > 2001:db8::1: UDP, length 12' tcpdump -q -tt -r "$TMPDIR/corpus.pcap"

echo "  ── -e MAC prefix on IPv6 line ──"
assert_cmd_pat 'aa:bb:cc:dd:ee:ff > ff:ff:ff:ff:ff:ff, 2001:db8::2\.33434 > 2001:db8::1\.53: UDP, length 12' tcpdump -e -tt -r "$TMPDIR/corpus.pcap"

echo "  ── -x hex dump format ──"
out=$("$MODBOX" tcpdump -tt -x -r "$TMPDIR/ipv4_tcp_syn.pcap" 2>/dev/null)
if printf '%s\n' "$out" | grep -qE '^0000  ff ff ff ff ff ff aa bb cc dd ee ff 08 00 45 00$'; then
    pass "-x first line format"
else
    fail "-x first line format: $(printf '%s' "$out" | head -2)"
fi
hexlines=$(printf '%s\n' "$out" | grep -cE '^00[0-9a-f]{2}  ')
if [[ $hexlines -eq 4 ]]; then pass "-x → 4 hex lines for 54 bytes"; else fail "-x → $hexlines hex lines"; fi
nb=$(printf '%s\n' "$out" | grep -E '^00[0-9a-f]{2}  ' | sed 's/^[0-9a-f]\{4\}  //' | grep -oE '[0-9a-f]{2}' | wc -l)
if [[ $nb -eq 54 ]]; then pass "-x dumps 54 bytes"; else fail "-x dumps $nb bytes"; fi

echo "  ── -s 34 truncates transport header → generic IPv4 line ──"
assert_cmd '1737102919.123456 192.168.1.2 > 192.168.1.1: IP, proto 6, length 0' tcpdump -tt -s 34 -r "$TMPDIR/ipv4_tcp_syn.pcap"

echo "  ── -s 5 → no output, exit 0 ──"
out=$("$MODBOX" tcpdump -tt -s 5 -r "$TMPDIR/ipv4_tcp_syn.pcap" 2>/dev/null); rc=$?
if [[ $rc -eq 0 && -z "$out" ]]; then pass "-s 5 → no output"; else fail "-s 5 → rc=$rc out=[$out]"; fi

echo "  ── -n and -nn output identical to default ──"
a=$("$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" 2>/dev/null)
b=$("$MODBOX" tcpdump -n -tt -r "$TMPDIR/corpus.pcap" 2>/dev/null)
c=$("$MODBOX" tcpdump -nn -tt -r "$TMPDIR/corpus.pcap" 2>/dev/null)
if [[ "$a" == "$b" && "$a" == "$c" ]]; then pass "-n/-nn → identical output"; else fail "-n/-nn → output differs"; fi

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
pcap_record "$TMPDIR/ipv6_short.pcap" 47168a67 40e20100 30000000 eb001122334400ab3c2d1e0086dd600000000014064020010db800000000000000000000000220010db8000000000000
out=$("$MODBOX" tcpdump -tt -r "$TMPDIR/ipv6_short.pcap" 2>/dev/null); rc=$?
if [[ $rc -eq 0 && -z "$out" ]]; then pass "truncated IPv6 → no output"; else fail "truncated IPv6 → rc=$rc out=[$out]"; fi

# ── pcap writer -w ─────────────────────────────────────────────────────────
echo "  ── -w round trip: decode output byte-identical ──"
"$MODBOX" tcpdump -r "$TMPDIR/corpus.pcap" -w "$TMPDIR/rt.pcap" >/dev/null 2>"$TMPDIR/werr"; rc=$?
if [[ $rc -ne 0 ]]; then
    fail "-w round trip → rc=$rc stderr=[$(cat "$TMPDIR/werr")]"
else
    "$MODBOX" tcpdump -tt -r "$TMPDIR/rt.pcap" > "$TMPDIR/rt.txt" 2>/dev/null
    "$MODBOX" tcpdump -tt -r "$TMPDIR/corpus.pcap" > "$TMPDIR/corpus.txt" 2>/dev/null
    if diff -q "$TMPDIR/rt.txt" "$TMPDIR/corpus.txt" > /dev/null; then
        pass "-w round trip → decode output identical"
    else
        fail "-w round trip → decode output differs"
    fi
fi

echo "  ── -w output magic is LE a1b2c3d4 ──"
magic=$(xxd -p -l 4 "$TMPDIR/rt.pcap" 2>/dev/null)
if [[ "$magic" == "a1b2c3d4" ]]; then pass "-w magic → a1b2c3d4"; else fail "-w magic → $magic"; fi

echo "  ── -w file size = 24 + 5*(16+incl) (garbage excluded) ──"
size=$(stat -c%s "$TMPDIR/rt.pcap" 2>/dev/null)
# incl: ARP 42, TCP 54, UDP 84, ICMP 46, IPv6/UDP 74 → 24 + 5*16 + 300 = 404
if [[ "$size" -eq 404 ]]; then pass "-w size → 404"; else fail "-w size → $size (expected 404)"; fi

echo "  ── -r and -w same path → exit 2 ──"
"$MODBOX" tcpdump -r "$TMPDIR/corpus.pcap" -w "$TMPDIR/corpus.pcap" >/dev/null 2>"$TMPDIR/werr"; rc=$?
if [[ $rc -eq 2 ]] && grep -q 'same file' "$TMPDIR/werr"; then
    pass "-r and -w same file → exit 2"
else
    fail "-r and -w same file → rc=$rc stderr=[$(cat "$TMPDIR/werr")]"
fi

echo "  ── -w to nonexistent dir → exit 1 ──"
"$MODBOX" tcpdump -r "$TMPDIR/corpus.pcap" -w "/nonexistent_dir/x.pcap" >/dev/null 2>"$TMPDIR/werr"; rc=$?
if [[ $rc -eq 1 ]] && grep -q 'cannot create' "$TMPDIR/werr"; then
    pass "-w nonexistent dir → exit 1"
else
    fail "-w nonexistent dir → rc=$rc stderr=[$(cat "$TMPDIR/werr")]"
fi

echo "  ── -w with filter: only kept packets written ──"
"$MODBOX" tcpdump -r "$TMPDIR/corpus.pcap" -w "$TMPDIR/udp.pcap" -f 'udp' >/dev/null 2>/dev/null
"$MODBOX" tcpdump -tt -r "$TMPDIR/udp.pcap" > "$TMPDIR/udp.txt" 2>/dev/null
udp_lines=$(grep -c 'UDP, length' "$TMPDIR/udp.txt")
if [[ $udp_lines -eq 2 ]]; then pass "-w + filter → 2 UDP records"; else fail "-w + filter → $udp_lines lines"; fi

echo "  ── -r stdin -w file ──"
"$MODBOX" tcpdump -r - -w "$TMPDIR/stdin.pcap" < "$TMPDIR/corpus.pcap" >/dev/null 2>/dev/null; rc=$?
"$MODBOX" tcpdump -tt -r "$TMPDIR/stdin.pcap" > "$TMPDIR/stdin.txt" 2>/dev/null
stdin_lines=$(grep -c . "$TMPDIR/stdin.txt")
if [[ $rc -eq 0 && $stdin_lines -eq 5 ]]; then pass "-r - -w file → 5 lines"; else fail "-r - -w file → rc=$rc lines=$stdin_lines"; fi
