SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── watch ───────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' watch --help

echo "  ── -h shows usage ──"
assert_cmd_pat 'Usage:' watch -h

echo "  ── --version shows version ──"
assert_cmd_pat 'watch \(modbox\)' watch --version

echo "  ── -v shows version ──"
assert_cmd_pat 'watch \(modbox\)' watch -v

echo "  ── missing command errors ──"
assert_cmd_pat_stderr 'missing command' watch
"$MODBOX" watch >/dev/null 2>&1
if [[ "$?" -eq 2 ]]; then
    pass "watch with no command exits 2"
else
    fail "watch with no command — expected exit 2"
fi

echo "  ── unknown option errors ──"
assert_cmd_pat_stderr "unrecognized option '--bogus'" watch --bogus echo hi
"$MODBOX" watch --bogus echo hi >/dev/null 2>&1
if [[ "$?" -eq 2 ]]; then
    pass "watch --bogus exits 2"
else
    fail "watch --bogus — expected exit 2"
fi

echo "  ── invalid short option errors ──"
assert_cmd_pat_stderr "invalid option -- 'Z'" watch -Z echo hi

echo "  ── invalid interval (non-numeric) errors ──"
assert_cmd_pat_stderr 'invalid interval' watch -n abc echo hi
"$MODBOX" watch -n abc echo hi >/dev/null 2>&1
if [[ "$?" -eq 2 ]]; then
    pass "watch -n abc exits 2"
else
    fail "watch -n abc — expected exit 2"
fi

echo "  ── invalid interval (zero) errors ──"
assert_cmd_pat_stderr 'invalid interval' watch -n 0 echo hi
"$MODBOX" watch -n 0 echo hi >/dev/null 2>&1
if [[ "$?" -eq 2 ]]; then
    pass "watch -n 0 exits 2"
else
    fail "watch -n 0 — expected exit 2"
fi

echo "  ── absurd magnitude interval rejected ──"
assert_cmd_pat_stderr 'invalid interval' watch -n 999999999d echo hi

echo "  ── -n without value errors ──"
assert_cmd_pat_stderr "requires an argument" watch -n

echo "  ── unknown --differences value errors ──"
assert_cmd_pat_stderr "invalid argument 'bogus' for '--differences'" watch --differences=bogus echo hi

echo "  ── runs command repeatedly with title (non-tty, no ESC) ──"
watch_out=$("$MODBOX" timeout 1 "$MODBOX" watch -n 0.1 echo hello 2>/dev/null || true)
if printf '%s' "$watch_out" | grep -qE 'Every 0\.1s: echo hello'; then
    pass "watch -n 0.1 echo hello → title matches"
else
    fail "watch -n 0.1 echo hello — expected title 'Every 0.1s: echo hello'"
fi
hello_count=$(printf '%s\n' "$watch_out" | grep -c '^hello$' || true)
if [[ "$hello_count" -ge 2 ]]; then
    pass "watch repeats the command (found $hello_count hello lines)"
else
    fail "watch repeats the command — expected >= 2 hello lines, got $hello_count"
fi
if printf '%s' "$watch_out" | grep -q $'\x1b'; then
    fail "non-tty watch output must not contain ESC sequences"
else
    pass "non-tty watch output has no ESC sequences"
fi

echo "  ── -t hides the title ──"
watch_t_out=$("$MODBOX" timeout 1 "$MODBOX" watch -t -n 0.1 echo hi 2>/dev/null || true)
if printf '%s' "$watch_t_out" | grep -q '^hi$'; then
    pass "watch -t still runs the command"
else
    fail "watch -t — expected command output 'hi'"
fi
if printf '%s' "$watch_t_out" | grep -q 'Every'; then
    fail "watch -t — title must be suppressed"
else
    pass "watch -t suppresses the title"
fi

echo "  ── sh -c join semantics ──"
sh_out=$("$MODBOX" timeout 1 "$MODBOX" watch -n 0.1 'echo $((40+2))' 2>/dev/null || true)
if printf '%s' "$sh_out" | grep -q '^42$'; then
    pass "watch runs args via sh -c (arithmetic evaluated)"
else
    fail "watch sh -c — expected arithmetic output '42'"
fi

echo "  ── -e errexit exits with command status ──"
"$MODBOX" timeout 5 "$MODBOX" watch -e -n 0.1 false >/dev/null 2>&1
rc=$?
if [[ "$rc" -eq 1 ]]; then
    pass "watch -e false exits 1"
else
    fail "watch -e false — expected exit 1, got $rc"
fi

"$MODBOX" timeout 5 "$MODBOX" watch -x -e -n 0.1 sh -c 'exit 3' >/dev/null 2>&1
rc=$?
if [[ "$rc" -eq 3 ]]; then
    pass "watch -x -e propagates command exit status 3"
else
    fail "watch -x -e exit 3 — expected 3, got $rc"
fi

echo "  ── -g chgexit exits 0 on output change ──"
"$MODBOX" timeout 5 "$MODBOX" watch -g -n 0.1 date +%s%N >/dev/null 2>&1
rc=$?
if [[ "$rc" -eq 0 ]]; then
    pass "watch -g exits 0 when output changes"
else
    fail "watch -g — expected exit 0, got $rc"
fi

echo "  ── -g stays running when output is stable ──"
"$MODBOX" timeout 1 "$MODBOX" watch -g -n 0.2 echo same >/dev/null 2>&1
rc=$?
if [[ "$rc" -eq 124 ]]; then
    pass "watch -g with stable output keeps running"
else
    fail "watch -g stable output — expected timeout 124, got $rc"
fi

echo "  ── -b beeps on non-zero exit ──"
beep_out=$("$MODBOX" timeout 5 "$MODBOX" watch -b -e -n 0.1 false 2>/dev/null || true)
if printf '%s' "$beep_out" | grep -q $'\x07'; then
    pass "watch -b -e emits BEL on non-zero exit"
else
    fail "watch -b -e — expected BEL character in output"
fi

echo "  ── -d accepted, clean output on non-tty ──"
d_out=$("$MODBOX" timeout 1 "$MODBOX" watch -d -n 0.1 echo hi 2>/dev/null || true)
if printf '%s' "$d_out" | grep -q '^hi$'; then
    pass "watch -d runs the command"
else
    fail "watch -d — expected command output 'hi'"
fi
if printf '%s' "$d_out" | grep -q $'\x1b'; then
    fail "watch -d non-tty output must not contain styling"
else
    pass "watch -d non-tty output has no styling"
fi

echo "  ── -d permanent accepted ──"
dp_out=$("$MODBOX" timeout 1 "$MODBOX" watch -dpermanent -n 0.1 echo hi 2>/dev/null || true)
if printf '%s' "$dp_out" | grep -q '^hi$'; then
    pass "watch -dpermanent runs the command"
else
    fail "watch -dpermanent — expected command output 'hi'"
fi

echo "  ── interval suffix (0.1s) accepted ──"
sfx_out=$("$MODBOX" timeout 1 "$MODBOX" watch -n 0.1s echo hi 2>/dev/null || true)
if printf '%s' "$sfx_out" | grep -q 'Every 0\.1s: echo hi'; then
    pass "watch -n 0.1s accepted with s suffix"
else
    fail "watch -n 0.1s — expected title 'Every 0.1s: echo hi'"
fi

echo "  ── -x exec mode ──"
x_out=$("$MODBOX" timeout 1 "$MODBOX" watch -x -n 0.1 echo hi 2>/dev/null || true)
if printf '%s' "$x_out" | grep -q 'Every 0\.1s: echo hi'; then
    pass "watch -x runs the command via execvp"
else
    fail "watch -x — expected title 'Every 0.1s: echo hi'"
fi

echo "  ── -- terminates option parsing ──"
dd_out=$("$MODBOX" timeout 1 "$MODBOX" watch -n 0.1 -- -n 0.5 2>/dev/null || true)
if printf '%s' "$dd_out" | grep -qE 'Every 0\.1s: -n 0\.5'; then
    pass "watch -- passes later options to the command (interval unchanged)"
else
    fail "watch -- — expected title 'Every 0.1s: -n 0.5'"
fi

echo "  ── watch appears in modbox help ──"
assert_cmd_pat 'watch' help
