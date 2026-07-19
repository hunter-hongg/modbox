SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── timeout ───────────────────────────────────"

echo "  ── command completes before duration ──"
"$MODBOX" timeout 5 true
rc=$?
if [[ $rc -eq 0 ]]; then
    pass "timeout (command finishes)"
else
    fail "timeout — expected exit 0, got [$rc]"
fi

echo "  ── timed out command exits 124 ──"
"$MODBOX" timeout 1 sleep 3
rc=$?
if [[ $rc -eq 124 ]]; then
    pass "timeout (exit 124 on timeout)"
else
    fail "timeout — expected exit 124, got [$rc]"
fi

echo "  ── exit status passes through ──"
"$MODBOX" timeout 5 sh -c 'exit 7'
rc=$?
if [[ $rc -eq 7 ]]; then
    pass "timeout (exit passthrough 7)"
else
    fail "timeout — expected exit 7, got [$rc]"
fi

echo "  ── --preserve-status keeps child status ──"
"$MODBOX" timeout --preserve-status 1 sh -c 'exit 7'
rc=$?
if [[ $rc -eq 7 ]]; then
    pass "timeout --preserve-status (exit 7)"
else
    fail "timeout --preserve-status — expected exit 7, got [$rc]"
fi

echo "  ── --preserve-status keeps 124 on timeout ──"
"$MODBOX" timeout --preserve-status 1 sleep 3
rc=$?
if [[ $rc -eq 124 ]]; then
    pass "timeout --preserve-status (124 on timeout)"
else
    fail "timeout --preserve-status — expected exit 124, got [$rc]"
fi

echo "  ── nonexistent command exits 127 ──"
"$MODBOX" timeout 5 nosuchcmd_modbox_xyz
rc=$?
if [[ $rc -eq 127 ]]; then
    pass "timeout nosuchcmd (exit 127)"
else
    fail "timeout nosuchcmd — expected exit 127, got [$rc]"
fi

echo "  ── missing operand errors ──"
"$MODBOX" timeout >/dev/null 2>&1
rc=$?
if [[ $rc -eq 125 ]]; then
    pass "timeout (missing operand → 125)"
else
    fail "timeout — expected exit 125, got [$rc]"
fi

echo "  ── missing command errors ──"
"$MODBOX" timeout 5 >/dev/null 2>&1
rc=$?
if [[ $rc -eq 125 ]]; then
    pass "timeout (missing command → 125)"
else
    fail "timeout — expected exit 125, got [$rc]"
fi

echo "  ── invalid duration errors ──"
"$MODBOX" timeout bogus sleep 1 >/dev/null 2>&1
rc=$?
if [[ $rc -eq 125 ]]; then
    pass "timeout (invalid duration → 125)"
else
    fail "timeout — expected exit 125, got [$rc]"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage:' timeout --help

echo "  ── --version ──"
assert_cmd_pat 'timeout \(modbox\) 1.0' timeout --version

echo "  ── --foreground ignores timeout of children semantics ──"
"$MODBOX" timeout --foreground 1 sleep 3
rc=$?
if [[ $rc -eq 124 ]]; then
    pass "timeout --foreground (exit 124 on timeout)"
else
    fail "timeout --foreground — expected exit 124, got [$rc]"
fi

echo "  ── --kill-after sends SIGKILL ──"
"$MODBOX" timeout --signal=STOP --kill-after=1 1 sleep 5
rc=$?
if [[ $rc -eq 124 ]]; then
    pass "timeout --kill-after (exit 124)"
else
    fail "timeout --kill-after — expected exit 124, got [$rc]"
fi

echo "  ── fractional duration works ──"
start=$(date +%s.%N)
"$MODBOX" timeout 0.3 sleep 5 >/dev/null 2>&1
rc=$?
end=$(date +%s.%N)
elapsed=$(awk "BEGIN{print $end-$start}")
if [[ $rc -eq 124 ]] && awk "BEGIN{exit !($elapsed < 1.5)}"; then
    pass "timeout 0.3s (exit 124, fast)"
else
    fail "timeout 0.3s — expected exit 124 quickly, got rc=$rc elapsed=$elapsed"
fi
