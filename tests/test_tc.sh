SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tc ────────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' tc --help

echo "  ── --version shows version ──"
assert_cmd_pat 'tc \(modbox\) 1\.0' tc --version

echo "  ── no object errors with non-zero exit ──"
"$MODBOX" tc >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "tc (no args) → non-zero exit"; else fail "tc (no args) → expected non-zero, got 0"; fi

echo "  ── unknown object errors ──"
assert_cmd_pat_stderr 'unknown object' tc bogus

echo "  ── unexpected argument (positional) errors ──"
assert_cmd_pat_stderr 'unexpected argument' tc qdisc show bogusarg

echo "  ── unrecognized option errors ──"
assert_cmd_pat_stderr 'unrecognized option' tc --nope

echo "  ── -stats / -details long-form aliases accepted ──"
assert_cmd_pat 'qdisc .* dev lo ' tc -stats qdisc show dev lo
assert_cmd_pat 'qdisc .* dev lo ' tc -details qdisc show dev lo

echo "  ── unknown device errors ──"
assert_cmd_pat_stderr 'no such device' tc qdisc show dev nonexistent0

echo "  ── qdisc show prints kernel qdiscs ──"
assert_cmd_pat 'qdisc' tc qdisc show
assert_cmd_pat 'dev' tc qdisc show
assert_cmd_pat 'root' tc qdisc show
assert_cmd_pat 'refcnt' tc qdisc show

echo "  ── qdisc show dev lo scopes to loopback ──"
assert_cmd_pat 'qdisc .* dev lo ' tc qdisc show dev lo
assert_cmd_not_pat 'enp0s31f6' tc qdisc show dev lo

echo "  ── qdisc show dev lo -s shows statistics ──"
assert_cmd_pat 'Sent' tc qdisc show dev lo -s
assert_cmd_pat 'bytes' tc qdisc show dev lo -s
assert_cmd_pat 'dropped' tc qdisc show dev lo -s

echo "  ── class and filter show run unprivileged ──"
"$MODBOX" tc class show >/dev/null 2>&1; rc=$?
if [[ $rc -eq 0 ]]; then pass "tc class show → exit 0"; else fail "tc class show → expected 0, got $rc"; fi
"$MODBOX" tc filter show >/dev/null 2>&1; rc=$?
if [[ $rc -eq 0 ]]; then pass "tc filter show → exit 0"; else fail "tc filter show → expected 0, got $rc"; fi

echo "  ── -json qdisc show emits valid JSON ──"
tc_json=$("$MODBOX" tc -json qdisc show dev lo)
echo "$tc_json" | "$MODBOX" jq -c 'type=="array" and (.[0]|has("kind")) and (.[0].dev=="lo")' >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "tc -json qdisc show dev lo → valid JSON array with dev=lo"; else fail "tc -json qdisc show dev lo → invalid or missing JSON"; fi

echo "  ── -json -s qdisc show includes stats block ──"
tc_s_json=$("$MODBOX" tc -s -json qdisc show dev lo)
echo "$tc_s_json" | "$MODBOX" jq -c '.[0].stats and (.[0].stats|has("qlen"))' >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "tc -s -json qdisc show dev lo → stats block present"; else fail "tc -s -json qdisc show dev lo → missing stats block"; fi

echo "  ── -json -pretty qdisc show still valid JSON ──"
"$MODBOX" tc -s -json -pretty qdisc show dev lo | "$MODBOX" jq -c 'type=="array"' >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "tc -s -json -pretty qdisc show dev lo → valid JSON"; else fail "tc -s -json -pretty qdisc show dev lo → invalid JSON"; fi
