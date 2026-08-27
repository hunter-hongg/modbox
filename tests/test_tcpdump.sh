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
