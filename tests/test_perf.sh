#!/usr/bin/env bash
#
# test_perf.sh — Tests for modbox perf command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── perf ────────────────────────────────────────────────────────────────────"

echo "  ── basic execution ──"
assert_cmd_pat_stderr 'Performance counter stats' perf stat true

echo "  ── exit status propagation (true) ──"
"$MODBOX" perf stat true >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "perf stat true exits 0"
else
    fail "perf stat true — expected exit 0"
fi

echo "  ── exit status propagation (false) ──"
"$MODBOX" perf stat false >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "perf stat false exits 1"
else
    fail "perf stat false — expected exit 1, got $?"
fi

echo "  ── default events include task-clock ──"
result=$("$MODBOX" perf stat true 2>&1)
if echo "$result" | grep -q 'task-clock'; then
    pass "perf stat default events include task-clock"
else
    fail "perf stat — expected task-clock in output, got: $result"
fi

echo "  ── single event -e cycles ──"
result=$("$MODBOX" perf stat -e cycles true 2>&1)
if echo "$result" | grep -q 'cycles'; then
    pass "perf stat -e cycles shows cycles"
else
    fail "perf stat -e cycles — expected cycles in output"
fi

echo "  ── multiple events -e cycles,instructions ──"
result=$("$MODBOX" perf stat -e cycles,instructions true 2>&1)
if echo "$result" | grep -q 'cycles' && echo "$result" | grep -q 'instructions'; then
    pass "perf stat -e cycles,instructions shows both"
else
    fail "perf stat -e cycles,instructions — expected both events"
fi

echo "  ── --format filters to only cycles ──"
result=$("$MODBOX" perf stat -e cycles,instructions --format cycles true 2>&1)
if echo "$result" | grep -q 'cycles' && ! echo "$result" | grep -q 'instructions'; then
    pass "perf stat --format cycles shows only cycles"
else
    fail "perf stat --format cycles — expected only cycles"
fi

echo "  ── --null omits header ──"
result=$("$MODBOX" perf stat --null true 2>&1)
if ! echo "$result" | grep -q 'Performance counter stats'; then
    pass "perf stat --null omits header"
else
    fail "perf stat --null — expected no header line"
fi

echo "  ── --csv produces pipe-delimited output ──"
result=$("$MODBOX" perf stat --csv true 2>&1)
if echo "$result" | grep -q '|'; then
    pass "perf stat --csv has pipe delimiter"
else
    fail "perf stat --csv — expected pipe-delimited output"
fi

echo "  ── -o writes to file ──"
outfile="$TMPDIR/perf_out.txt"
"$MODBOX" perf stat -o "$outfile" true >/dev/null 2>&1
if grep -q 'Performance counter stats' "$outfile" || grep -q 'task-clock' "$outfile"; then
    pass "perf stat -o writes to file"
else
    fail "perf stat -o — expected output in $outfile"
fi

echo "  ── --all-cpus runs without error ──"
"$MODBOX" perf stat --all-cpus true >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "perf stat --all-cpus exits 0"
else
    fail "perf stat --all-cpus — expected exit 0, got $?"
fi

echo "  ── --repeat 2 runs twice ──"
result=$("$MODBOX" perf stat --repeat 2 true 2>&1)
# Should contain results from both runs (elapsed time appears twice or average shown)
count=$(echo "$result" | grep -c 'seconds time elapsed')
if [[ "$count" -ge 1 ]]; then
    pass "perf stat --repeat 2 produces output"
else
    fail "perf stat --repeat 2 — expected at least one elapsed line"
fi

echo "  ── nonexistent command exits 127 ──"
"$MODBOX" perf stat nosuchcmd_modbox_xyz >/dev/null 2>&1
if [[ $? -eq 127 ]]; then
    pass "perf stat nonexistent exits 127"
else
    fail "perf stat nonexistent — expected exit 127, got $?"
fi

echo "  ── no command errors ──"
assert_cmd_pat_stderr 'missing command' perf stat

echo "  ── list shows hardware and software events ──"
result=$("$MODBOX" perf list 2>&1)
if echo "$result" | grep -q 'Hardware event' && echo "$result" | grep -q 'Software event'; then
    pass "perf list shows both categories"
else
    fail "perf list — expected Hardware event and Software event sections"
fi

echo "  ── list hw shows only hardware ──"
result=$("$MODBOX" perf list hw 2>&1)
if echo "$result" | grep -q 'Hardware event' && ! echo "$result" | grep -q 'Software event'; then
    pass "perf list hw shows only hardware"
else
    fail "perf list hw — expected only hardware events"
fi

echo "  ── list sw shows only software ──"
result=$("$MODBOX" perf list sw 2>&1)
if echo "$result" | grep -q 'Software event' && ! echo "$result" | grep -q 'Hardware event'; then
    pass "perf list sw shows only software"
else
    fail "perf list sw — expected only software events"
fi

echo "  ── record is not implemented ──"
assert_cmd_pat_stderr 'not implemented' perf record

echo "  ── report is not implemented ──"
assert_cmd_pat_stderr 'not implemented' perf report

echo "  ── unknown subcommand errors ──"
assert_cmd_pat_stderr "is not a valid subcommand" perf unknown

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' perf --help

echo "  ── --version shows version ──"
assert_cmd_pat 'perf \(modbox\) 1.0' perf --version

echo ""
echo "=== perf Tests Complete ==="
