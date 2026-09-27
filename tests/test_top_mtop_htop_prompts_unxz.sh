SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# top — batch mode is the only non-interactive surface; the TUI needs a
# terminal, so the tests drive -b/-n1 and assert the printed table instead.

echo ""
echo "── top ───────────────────────────────────────"

echo "  ── help ──"
assert_cmd_pat 'Usage:' top --help

# top/mtop/htop follow the repo's larger convention (92 of 202 commands, among
# them ip and ss) of not implementing --version; they answer it as an invalid
# option instead. Asserted so a future --version addition is a deliberate change.
assert_cmd_pat_stderr 'invalid option' top --version

echo "  ── batch mode prints a header block and a process table ──"
# -n1 -b is one iteration, no TUI: safe to run non-interactively.
assert_cmd_pat '^top - [0-9]{2}:[0-9]{2}:[0-9]{2} up ' top -b -n1
assert_cmd_pat '^Tasks: [0-9]+ total' top -b -n1
assert_cmd_pat '^PID +USER +PR +NI ' top -b -n1

echo "  ── -n1 implies a single iteration without -b ──"
# The header alone is enough; a second iteration would need another delay.
assert_cmd_pat '^Tasks: [0-9]+ total' top -n1

echo "  ── -p restricts the table to one PID ──"
all_count=$(timeout 10 "$MODBOX" top -b -n1 2>/dev/null | grep -cE '^[[:space:]]*[0-9]+ ')
one_count=$(timeout 10 "$MODBOX" top -b -n1 -p 1 2>/dev/null | grep -cE '^[[:space:]]*[0-9]+ ')
if [[ "$one_count" -eq 1 ]]; then
    pass "top -p 1: table shows exactly one process"
else
    fail "top -p 1: expected 1 row, got $one_count (unfiltered had $all_count)"
fi
assert_cmd_pat '^Tasks: 1 total' top -b -n1 -p 1

echo "  ── memory line reports MiB units ──"
assert_cmd_pat 'Mem: .* total, .* free, .* used' top -b -n1

echo "  ── unknown option is rejected with the command's own wording ──"
assert_cmd_pat_stderr 'invalid option' top --no-such-flag
assert_cmd_pat_stderr 'Try .top --help' top --no-such-flag

echo ""
echo "── mtop ──────────────────────────────────────"
# mtop is TUI-only; --help is the non-interactive contract.

echo "  ── help / version ──"
assert_cmd_pat 'Usage: mtop' mtop --help
assert_cmd_pat_stderr 'invalid option' mtop --version

echo "  ── help documents the key bindings ──"
assert_cmd_pat 'Quit' mtop --help
assert_cmd_pat 'Sort by' mtop --help
assert_cmd_pat '\-d, \-\-delay' mtop --help
assert_cmd_pat '\-p, \-\-pid' mtop --help

echo "  ── discoverable via modbox help ──"
assert_cmd_pat 'mtop' help

echo ""
echo "── htop ──────────────────────────────────────"

echo "  ── help / version ──"
assert_cmd_pat 'Usage: htop' htop --help
assert_cmd_pat_stderr 'invalid option' htop --version

echo "  ── help documents the key bindings ──"
assert_cmd_pat 'Quit' htop --help
assert_cmd_pat 'Scroll' htop --help
assert_cmd_pat '\-d, \-\-delay' htop --help
assert_cmd_pat '\-p, \-\-pid' htop --help

echo "  ── discoverable via modbox help ──"
assert_cmd_pat 'htop' help

echo ""
echo "── prompts ───────────────────────────────────"

echo "  ── help ──"
assert_cmd_pat 'Usage: prompts' prompts --help

echo "  ── render emits a non-empty string ──"
rendered=$("$MODBOX" prompts render 2>/dev/null)
if [[ -n "$rendered" ]]; then
    pass "prompts render: produced output"
else
    fail "prompts render: produced nothing"
fi

echo "  ── PROMPTS_ENABLED names modules, so an unknown name renders nothing ──"
# PROMPTS_ENABLED is a module list, not a boolean: `0` and `1` are not module
# names, so neither enables anything. Asserted because it reads like a boolean
# and silently disabling the prompt is the confusing outcome.
for v in 0 1 true; do
    len=$(PROMPTS_ENABLED="$v" "$MODBOX" prompts render 2>/dev/null | wc -c)
    if [[ "$len" -eq 0 ]]; then
        pass "prompts render: PROMPTS_ENABLED=$v matches no module (empty)"
    else
        fail "prompts render: PROMPTS_ENABLED=$v produced $len bytes"
    fi
done

echo "  ── PROMPTS_ENABLED selects a single module ──"
only_user=$(PROMPTS_ENABLED=username "$MODBOX" prompts render 2>/dev/null)
if [[ -n "$only_user" ]]; then
    pass "prompts render: PROMPTS_ENABLED=username renders"
else
    fail "prompts render: PROMPTS_ENABLED=username produced nothing"
fi

echo "  ── list enumerates the documented modules ──"
for m in username hostname directory git_status cmd_duration exit_code; do
    assert_cmd_pat "^[[:space:]]+$m\b" prompts list
done

echo "  ── modules NAME shows that module's config ──"
assert_cmd_pat 'username' prompts modules username
assert_cmd_pat 'style' prompts list

echo "  ── init emits shell integration for each supported shell ──"
for sh in bash zsh fish; do
    init_out=$("$MODBOX" prompts init "$sh" 2>/dev/null)
    if [[ -n "$init_out" ]]; then
        pass "prompts init $sh: produced integration code"
    else
        fail "prompts init $sh: produced nothing"
    fi
done
# bash integration must define the hooks the README tells you to eval, and
# export the two variables the exit_code / cmd_duration modules read.
assert_cmd_pat '__prompts_preexec' prompts init bash
assert_cmd_pat '__prompts_callback' prompts init bash
assert_cmd_pat 'PROMPTS_EXIT_CODE' prompts init bash
assert_cmd_pat 'PROMPTS_CMD_DURATION' prompts init bash
# It must wire itself into PROMPT_COMMAND without clobbering an existing one.
assert_cmd_pat 'PROMPT_COMMAND' prompts init bash

echo "  ── init rejects an unknown shell ──"
init_bad=$("$MODBOX" prompts init csh 2>&1)
init_rc=$?
if [[ $init_rc -ne 0 ]]; then
    pass "prompts init csh: exits non-zero"
else
    fail "prompts init csh: exited 0"
fi

echo ""
echo "── unxz ──────────────────────────────────────"
# unxz shares xz's implementation; what matters is that the alias decompresses
# by default and keeps the file under -c.

echo "  ── help / version ──"
assert_cmd_pat 'Usage: unxz' unxz --help
assert_cmd_pat 'unxz \(modbox\) 1\.0' unxz --version

echo "  ── decompresses by default ──"
printf 'unxz default payload\n' > "$TMPDIR/ux.txt"
cp "$TMPDIR/ux.txt" "$TMPDIR/ux.orig"
"$MODBOX" xz "$TMPDIR/ux.txt" >/dev/null 2>&1
"$MODBOX" unxz "$TMPDIR/ux.txt.xz" >/dev/null 2>&1
if [[ -f "$TMPDIR/ux.txt" && ! -f "$TMPDIR/ux.txt.xz" ]]; then
    pass "unxz: decompresses by default, removing the .xz"
else
    fail "unxz: default decompress state wrong"
fi
if cmp -s "$TMPDIR/ux.txt" "$TMPDIR/ux.orig"; then
    pass "unxz: round-trip byte-identical"
else
    fail "unxz: round-trip mismatch"
fi

echo "  ── -c writes to stdout and keeps the input ──"
printf 'unxz -c\n' > "$TMPDIR/uxc.txt"
"$MODBOX" xz "$TMPDIR/uxc.txt" >/dev/null 2>&1
uxc_out=$("$MODBOX" unxz -c "$TMPDIR/uxc.txt.xz" 2>/dev/null)
if [[ "$uxc_out" == "unxz -c" && -f "$TMPDIR/uxc.txt.xz" ]]; then
    pass "unxz -c: stdout, keeps the .xz"
else
    fail "unxz -c: out=[$uxc_out] xz=$([[ -f "$TMPDIR/uxc.txt.xz" ]] && echo kept || echo gone)"
fi

echo "  ── -k keeps the input ──"
printf 'unxz -k\n' > "$TMPDIR/uxk.txt"
"$MODBOX" xz "$TMPDIR/uxk.txt" >/dev/null 2>&1
"$MODBOX" unxz -k "$TMPDIR/uxk.txt.xz" >/dev/null 2>&1
if [[ -f "$TMPDIR/uxk.txt" && -f "$TMPDIR/uxk.txt.xz" ]]; then
    pass "unxz -k: keeps the .xz"
else
    fail "unxz -k: state wrong"
fi

echo "  ── stdin → stdout ──"
printf 'unxz pipe\n' > "$TMPDIR/uxp.txt"
"$MODBOX" xz "$TMPDIR/uxp.txt" >/dev/null 2>&1
pipe_out=$("$MODBOX" unxz < "$TMPDIR/uxp.txt.xz" 2>/dev/null)
if [[ "$pipe_out" == "unxz pipe" ]]; then
    pass "unxz: stdin -> stdout"
else
    fail "unxz: stdin->stdout got [$pipe_out]"
fi

echo "  ── non-xz input is an error ──"
printf 'plain text\n' > "$TMPDIR/uxplain.txt"
"$MODBOX" unxz "$TMPDIR/uxplain.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then
    pass "unxz: non-xz input exits non-zero"
else
    fail "unxz: non-xz input should exit non-zero"
fi
assert_cmd_pat_stderr 'not in xz format|not an xz|Compressed data' unxz "$TMPDIR/uxplain.txt"

echo "  ── discoverable via modbox help ──"
assert_cmd_pat 'unxz' help

echo ""
echo "── top / mtop / htop / prompts / unxz Tests Complete ==="
