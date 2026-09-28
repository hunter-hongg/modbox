#!/usr/bin/env bash
#
# test_nc.sh — Test suite for nc/netcat command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── nc / netcat ───────────────────────────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' nc --help

echo "  ── --version shows modbox version ──"
assert_cmd_pat 'modbox' nc --version

echo "  ── -h shows usage ──"
assert_cmd_pat 'Usage:' nc -h

echo "  ── -V shows modbox version ──"
assert_cmd_pat 'modbox' nc -V

echo "  ── no args is a usage error (exit 2) ──"
"$MODBOX" nc >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "nc (no args) → exit 2"; else fail "nc (no args) → exit $rc, expected 2"; fi

echo "  ── --bogus is a usage error (exit 2, hint on stderr) ──"
"$MODBOX" nc --bogus >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "nc --bogus → exit 2"; else fail "nc --bogus → exit $rc, expected 2"; fi
assert_cmd_pat_stderr 'Try .*--help' nc --bogus

echo "  ── missing port on -l is a usage error (exit 2) ──"
"$MODBOX" nc -l >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "nc -l (no port) → exit 2"; else fail "nc -l (no port) → exit $rc, expected 2"; fi

echo "  ── unknown host is an error (exit 1) ──"
"$MODBOX" nc no-such-host-xyz.invalid 80 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "nc unknown host → exit 1"; else fail "nc unknown host → exit $rc, expected 1"; fi

echo "  ── -z closed port is non-zero with 'failed' on stderr ──"
CLOSED_PORT=$((8000 + (RANDOM % 10000)))
"$MODBOX" nc -z 127.0.0.1 "$CLOSED_PORT" >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "nc -z $CLOSED_PORT → exit non-zero"; else fail "nc -z $CLOSED_PORT → exit 0, expected non-zero"; fi
assert_cmd_pat_stderr 'failed' nc -z 127.0.0.1 "$CLOSED_PORT"

echo "  ── -z unknown host is non-zero ──"
"$MODBOX" nc -z no-such-host-xyz.invalid 80 >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "nc -z unknown host → exit non-zero"; else fail "nc -z unknown host → exit 0, expected non-zero"; fi

echo "  ── -w timeout on unreachable IP exits 1 ──"
timeout 10 "$MODBOX" nc -w 1 10.255.255.1 80 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "nc -w 1 10.255.255.1 → exit 1"; else fail "nc -w 1 10.255.255.1 → exit $rc, expected 1"; fi

# Listener helpers: a one-off `nc -z` probe cannot be used to detect the
# bind, because the completed probe connection makes a non-`-k` listener
# relay an empty session and exit. The listener keeps stdin open (sleep 8)
# so its relay loop never sees EOF mid-test.
LISTENER_OUT="$TMPDIR/nc_listen.out"

start_listener() {
    rm -f "$LISTENER_OUT"
    sleep 8 | "$MODBOX" nc "$@" > "$LISTENER_OUT" 2>/dev/null &
    LPID=$!
    sleep 0.5
}

stop_listener() {
    kill "$LPID" 2>/dev/null
    wait "$LPID" 2>/dev/null
    LPID=""
}

echo "  ── live loopback tests ──"
LIVE_PORT=""
for p in $(seq 19000 19999); do
    if ! "$MODBOX" nc -z 127.0.0.1 "$p" >/dev/null 2>&1; then
        LIVE_PORT="$p"
        break
    fi
done

if [[ -n $LIVE_PORT ]]; then
    P="$LIVE_PORT"

    # TCP round trip: listener relays client data to its stdout
    echo "  ── live TCP round-trip: listener receives client data ──"
    start_listener -l "$P"
    printf 'hello-nc\n' | "$MODBOX" nc 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]] && grep -q 'hello-nc' "$LISTENER_OUT"; then
        pass "TCP round-trip: listener relayed client data"
    else
        fail "TCP round-trip — client rc=$rc, listener output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── live -v prints 'succeeded!' ──"
    start_listener -l "$P"
    VOUT=$("$MODBOX" nc -v 127.0.0.1 "$P" </dev/null 2>&1)
    rc=$?
    stop_listener
    if [[ $rc -eq 0 && "$VOUT" == *"succeeded!"* ]]; then
        pass "nc -v → prints succeeded!"
    else
        fail "nc -v — rc=$rc, output [$(echo "$VOUT" | head -c 80)]"
    fi

    echo "  ── live -k accepts multiple sequential clients ──"
    start_listener -l -k "$P"
    printf 'client-one\n' | "$MODBOX" nc 127.0.0.1 "$P" >/dev/null 2>&1
    r1=$?
    printf 'client-two\n' | "$MODBOX" nc 127.0.0.1 "$P" >/dev/null 2>&1
    r2=$?
    stop_listener
    if [[ $r1 -eq 0 && $r2 -eq 0 ]] && grep -q 'client-one' "$LISTENER_OUT" && grep -q 'client-two' "$LISTENER_OUT"; then
        pass "nc -k -l accepted two sequential clients"
    else
        fail "nc -k -l — r1=$r1 r2=$r2, output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── live UDP client completes ──"
    start_listener -l -u "$P"
    printf 'ping\n' | "$MODBOX" nc -u 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]]; then
        pass "nc -u client → exit 0"
    else
        fail "nc -u client → exit $rc, expected 0"
    fi

    echo "  ── live -i paced lines all arrive at listener ──"
    start_listener -l "$P"
    printf 'line-a\nline-b\n' | "$MODBOX" nc -i 0.2 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]] && grep -q 'line-a' "$LISTENER_OUT" && grep -q 'line-b' "$LISTENER_OUT"; then
        pass "nc -i 0.2 → both paced lines received"
    else
        fail "nc -i 0.2 — rc=$rc, output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── live -c executes command over the connection ──"
    start_listener -l "$P"
    "$MODBOX" nc -c 'echo exec-ok' 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]] && grep -q 'exec-ok' "$LISTENER_OUT"; then
        pass "nc -c → command output relayed"
    else
        fail "nc -c — rc=$rc, output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── glued -c<cmd> executes after connect ──"
    start_listener -l "$P"
    "$MODBOX" nc -cdate 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]] && [[ -s "$LISTENER_OUT" ]]; then
        pass "nc -cdate → glued -c parsed and command ran"
    else
        fail "nc -cdate — rc=$rc, output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── negative-port syntax connects ──"
    start_listener -l "$P"
    printf 'neg\n' | "$MODBOX" nc 127.0.0.1 "-$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]]; then
        pass "nc 127.0.0.1 -$P (negative port) → exit 0"
    else
        fail "nc 127.0.0.1 -$P (negative port) → exit $rc, expected 0"
    fi

    echo "  ── glued short-option values parse ──"
    "$MODBOX" nc -w1 -z 127.0.0.1 "$CLOSED_PORT" >/dev/null 2>&1; rc=$?
    if [[ $rc -ne 0 ]]; then pass "nc -w1 -z → glued -w parsed"; else fail "nc -w1 -z → exit 0, expected non-zero"; fi
    "$MODBOX" nc -i0.2 -z 127.0.0.1 "$CLOSED_PORT" >/dev/null 2>&1; rc=$?
    if [[ $rc -ne 0 ]]; then pass "nc -i0.2 -z → glued -i parsed"; else fail "nc -i0.2 -z → exit 0, expected non-zero"; fi
    "$MODBOX" nc -p12345 -z 127.0.0.1 "$CLOSED_PORT" >/dev/null 2>&1; rc=$?
    if [[ $rc -ne 0 ]]; then pass "nc -p12345 -z → glued -p parsed"; else fail "nc -p12345 -z → exit 0, expected non-zero"; fi
    # The `netcat` alias runs the same code, so a divergence would be in
    # behaviour rather than in registration. Exercise it for real, not just
    # for --help.
    echo "  ── live netcat alias: TCP round-trip ──"
    start_listener -l "$P"
    printf 'hello-netcat\n' | "$MODBOX" netcat 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]] && grep -q 'hello-netcat' "$LISTENER_OUT"; then
        pass "netcat alias: listener relayed client data"
    else
        fail "netcat alias round-trip — client rc=$rc, listener output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── live netcat alias: -v reports succeeded ──"
    start_listener -l "$P"
    VOUT=$("$MODBOX" netcat -v 127.0.0.1 "$P" </dev/null 2>&1)
    rc=$?
    stop_listener
    if [[ $rc -eq 0 && "$VOUT" == *"succeeded!"* ]]; then
        pass "netcat -v → prints succeeded!"
    else
        fail "netcat -v — rc=$rc, output [$(echo "$VOUT" | head -c 80)]"
    fi

    echo "  ── live netcat alias: -z probes an open port ──"
    start_listener -l -k "$P"
    "$MODBOX" netcat -z 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]]; then
        pass "netcat -z → exit 0 against a listening port"
    else
        fail "netcat -z → exit $rc against a listening port, expected 0"
    fi

    echo "  ── live netcat alias: -c executes a command ──"
    # Like nc -c, the command's output reaches us through the listener's relay,
    # not on the client's own stdout.
    start_listener -l -k "$P"
    "$MODBOX" netcat -c 'echo netcat-exec' 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]] && grep -q 'netcat-exec' "$LISTENER_OUT"; then
        pass "netcat -c → command output relayed"
    else
        fail "netcat -c — rc=$rc, output [$(head -c 80 "$LISTENER_OUT")]"
    fi

    echo "  ── live netcat alias: UDP client completes ──"
    start_listener -l -u -k "$P"
    printf 'udp-netcat\n' | "$MODBOX" netcat -u 127.0.0.1 "$P" >/dev/null 2>&1
    rc=$?
    stop_listener
    if [[ $rc -eq 0 ]]; then
        pass "netcat -u → UDP client completed"
    else
        fail "netcat -u — rc=$rc"
    fi
else
    echo "  SKIP  live loopback tests (no free bindable port in 19000-19999)"
fi

echo "  ── netcat alias works ──"
assert_cmd_pat 'Usage:' netcat --help

echo "  ── netcat --version shows modbox ──"
assert_cmd_pat 'modbox' netcat --version

echo "  ── netcat -h shows usage ──"
assert_cmd_pat 'Usage:' netcat -h

echo "  ── diagnostics name the command the user invoked ──"
# The alias shares the implementation, so a hardcoded "nc:" prefix would name
# a command the user never typed. -z against a closed port is the reliable
# trigger: it always fails to connect.
CLOSED=$((19000 + (RANDOM % 900)))
assert_cmd_pat_stderr '^netcat: ' netcat -z 127.0.0.1 "$CLOSED"
assert_cmd_pat_stderr '^nc: ' nc -z 127.0.0.1 "$CLOSED"
assert_cmd_pat_stderr '^netcat: missing host or port' netcat
assert_cmd_pat_stderr '^nc: missing host or port' nc
# ... and must not leak the other name.
if "$MODBOX" netcat -z 127.0.0.1 "$CLOSED" 2>&1 >/dev/null | grep -q '^nc: '; then
    fail "netcat diagnostics must not say 'nc:'"
else
    pass "netcat diagnostics do not say 'nc:'"
fi

echo "  ── netcat rejects bad options the same way as nc ──"
"$MODBOX" netcat >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "netcat (no args) → exit 2"; else fail "netcat (no args) → exit $rc, expected 2"; fi
"$MODBOX" netcat --bogus >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "netcat --bogus → exit 2"; else fail "netcat --bogus → exit $rc, expected 2"; fi
"$MODBOX" netcat -l >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "netcat -l (no port) → exit 2"; else fail "netcat -l (no port) → exit $rc, expected 2"; fi
"$MODBOX" netcat no-such-host-xyz.invalid 80 >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "netcat unknown host → exit 1"; else fail "netcat unknown host → exit $rc, expected 1"; fi
# Usage errors mention the alias in the hint too.
assert_cmd_pat_stderr 'Try .*--help' netcat --bogus

echo "  ── nc appears in modbox help ──"
assert_cmd_pat 'nc' help

echo "  ── netcat appears in modbox help ──"
assert_cmd_pat 'netcat' help
