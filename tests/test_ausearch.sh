#!/usr/bin/env bash
#
# test_ausearch.sh — Tests for ausearch command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# Sample audit logs (synthetic). Four events:
#   E1 (1680000000.123:456): SYSCALL(uid=1000, comm=ls, syscall=59, success=yes) + AVC(name=secret)
#   E2 (1680000000.456:789): SYSCALL(uid=1001, comm=cat, syscall=2, success=no, key=access) + AVC(name=shadow)
#   E3 (1680000001.000:1000): USER_LOGIN(pid=4000, uid=0, auid=1000)
#   E4 (1680000002.000:1001): KERNEL
AUDIT_SAMPLE=$(cat <<'EOF'
type=SYSCALL msg=audit(1680000000.123:456): arch=c000003e syscall=59 success=yes exit=0 a0=7fff1234 a1=7fff5678 a2=7fff9abc a3=7fffdef0 items=2 ppid=1000 pid=2000 auid=1000 uid=1000 gid=1000 euid=1000 suid=1000 fsuid=1000 egid=1000 sgid=1000 fsgid=1000 tty=pts0 ses=1 comm="ls" exe="/bin/ls" key=(null)
type=AVC msg=audit(1680000000.123:456): avc:  denied  { read } for  pid=2000 comm="ls" name="secret" dev="rootfs" ino=12345 scontext=unconfined_u:unconfined_r:unconfined_t:s0-s0:c0.c1023 tcontext=system_u:object_r:shadow_t:s0 tclass=file permissive=0
type=SYSCALL msg=audit(1680000000.456:789): arch=c000003e syscall=2 success=no exit=-13 a0=7fffabcd a1=0 a2=0 a3=0 items=1 ppid=1000 pid=3000 auid=1001 uid=1001 gid=1001 euid=1001 suid=1001 fsuid=1001 egid=1001 sgid=1001 fsgid=1001 tty=pts1 ses=2 comm="cat" exe="/bin/cat" key="access"
type=AVC msg=audit(1680000000.456:789): avc:  denied  { write } for  pid=3000 comm="cat" name="shadow" dev="rootfs" ino=12346 scontext=unconfined_u:unconfined_r:unconfined_t:s0-s0:c0.c1023 tcontext=system_u:object_r:shadow_t:s0 tclass=file permissive=0
type=USER_LOGIN msg=audit(1680000001.000:1000): pid=4000 uid=0 auid=1000 ses=3 subj=system_u:system_r:crond_t:s0-s0:c0.c1023 msg='op=login id=1000 exe="/usr/sbin/cron" hostname=? addr=? terminal=cron res=success'
type=KERNEL msg=audit(1680000002.000:1001): audit_backlog=0
EOF
)

echo ""
echo "── ausearch ──────────────────────────────────────"

# ── Help & Version ───────────────────────────────────────────────────────────

echo "  ── --help ──"
assert_cmd_pat 'Usage: ausearch' ausearch --help

echo "  ── --version ──"
assert_cmd_pat 'ausearch \(modbox\)' ausearch --version

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'invalid option' ausearch --foo

# ── Input and basic parsing ──────────────────────────────────────────────────

echo "  ── no input on terminal ──"
if [[ -t 0 ]]; then
    assert_cmd_pat_stderr 'no input specified' ausearch
else
    echo "  SKIP — stdin is not a terminal in this context"
fi

echo "  ── -m alone lists message types ──"
assert_cmd_pat 'Valid message types' ausearch -m
assert_cmd_pat 'SYSCALL' ausearch -m

echo "  ── empty input ──"
output=$(echo "" | $MODBOX ausearch 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "empty input produces no output"
else
    fail "empty input — expected no output, got [$output]"
fi

echo "  ── event assembly: matching event prints all its records ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -m SYSCALL 2>/dev/null)
if [[ "$output" == *"type=SYSCALL"* ]] && [[ "$output" == *"type=AVC"* ]]; then
    pass "event with SYSCALL+AVC records prints both"
else
    fail "event assembly — unexpected output: [$output]"
fi

echo "  ── event assembly: four distinct stamps yield four events ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch 2>/dev/null)
count=$(printf '%s\n' "$output" | grep -c '^---- time->' || true)
if [[ "$count" -eq 4 ]]; then
    pass "4 distinct stamps → 4 events"
else
    fail "event count — expected 4 events, got $count"
fi

echo "  ── stamp format preserved ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --just-one 2>/dev/null)
if [[ "$output" == *"msg=audit(1680000000.123:456):"* ]]; then
    pass "default output keeps EPOCH.MSEC:SERIAL stamp"
else
    fail "stamp format — expected 'msg=audit(1680000000.123:456):', got: [$output]"
fi

# ── Message type filter ──────────────────────────────────────────────────────

echo "  ── -m SYSCALL matches SYSCALL events ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -m SYSCALL 2>/dev/null)
if [[ -n "$output" ]] && [[ "$output" == *"type=SYSCALL"* ]]; then
    pass "-m SYSCALL matches"
else
    fail "-m SYSCALL — expected output, got: [$output]"
fi

echo "  ── -m USER_LOGIN excludes SYSCALL events ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -m USER_LOGIN 2>/dev/null)
if [[ "$output" == *"type=USER_LOGIN"* ]] && [[ "$output" != *"type=SYSCALL"* ]]; then
    pass "-m USER_LOGIN matches only login event"
else
    fail "-m USER_LOGIN — unexpected output: [$output]"
fi

echo "  ── -m ALL matches every event ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -m ALL 2>/dev/null)
count=$(printf '%s\n' "$output" | grep -c '^---- time->' || true)
if [[ "$count" -eq 4 ]]; then
    pass "-m ALL returns all 4 events"
else
    fail "-m ALL — expected 4 events, got $count"
fi

echo "  ── unknown message type errors ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -m BOGUS 2>&1)
rc=$?
if [[ $rc -ne 0 ]] && [[ "$output" == *"unknown message type"* ]]; then
    pass "unknown message type rejected"
else
    fail "unknown type — expected non-zero exit and error, got rc=$rc output=[$output]"
fi

# ── UID filtering ─────────────────────────────────────────────────────────────

echo "  ── --uid filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --uid 1000 2>/dev/null)
if [[ "$output" == *"uid=1000"* ]]; then
    pass "--uid 1000 matches"
else
    fail "--uid 1000 — expected output, got: [$output]"
fi

echo "  ── --uid filter excludes ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --uid 9999 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "--uid 9999 excluded"
else
    fail "--uid 9999 — expected no output, got: [$output]"
fi

echo "  ── --loginuid filter ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --loginuid 1000 2>/dev/null)
if [[ "$output" == *"auid=1000"* ]]; then
    pass "--loginuid 1000 matches"
else
    fail "--loginuid — expected output, got: [$output]"
fi

echo "  ── --uid accepts a user name ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --uid root 2>/dev/null)
if [[ "$output" == *"type=USER_LOGIN"* ]] && [[ "$output" == *"uid=0"* ]]; then
    pass "--uid root resolves to uid 0"
else
    fail "--uid root — expected USER_LOGIN event, got: [$output]"
fi

echo "  ── --uid rejects unresolvable name ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --uid nosuchuser12345 2>&1)
rc=$?
if [[ $rc -ne 0 ]] && [[ "$output" == *"cannot resolve uid"* ]]; then
    pass "unresolvable uid name rejected"
else
    fail "unresolvable uid — expected error, got rc=$rc output=[$output]"
fi

echo "  ── --uid-all matches value across uid fields ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --uid-all 1000 2>/dev/null)
count=$(printf '%s\n' "$output" | grep -c '^---- time->' || true)
if [[ "$count" -eq 2 ]]; then
    pass "--uid-all 1000 matches events with uid/euid/auid/suid=1000"
else
    fail "--uid-all — expected 2 events, got $count"
fi

# ── PID filtering ─────────────────────────────────────────────────────────────

echo "  ── -p filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -p 2000 2>/dev/null)
if [[ "$output" == *"pid=2000"* ]]; then
    pass "-p 2000 matches"
else
    fail "-p 2000 — expected output, got: [$output]"
fi

echo "  ── -p filter excludes ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -p 9999 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "-p 9999 excluded"
else
    fail "-p 9999 — expected no output, got: [$output]"
fi

# ── Comm filter ──────────────────────────────────────────────────────────────

echo "  ── -c filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -c ls 2>/dev/null)
if [[ "$output" == *"comm=\"ls\""* ]]; then
    pass "-c ls matches"
else
    fail "-c ls — expected output, got: [$output]"
fi

echo "  ── -c filter excludes ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -c nonexistent 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "-c nonexistent excluded"
else
    fail "-c nonexistent — expected no output, got: [$output]"
fi

# ── Executable filter ─────────────────────────────────────────────────────────

echo "  ── -x filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -x /bin/ls 2>/dev/null)
if [[ "$output" == *"/bin/ls"* ]]; then
    pass "-x /bin/ls matches"
else
    fail "-x — expected output, got: [$output]"
fi

# ── Syscall filter ────────────────────────────────────────────────────────────

echo "  ── --syscall filter by name ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --syscall execve 2>/dev/null)
if [[ "$output" == *"syscall=59"* ]]; then
    pass "--syscall execve resolves to 59 on x86_64"
else
    fail "--syscall execve — expected output, got: [$output]"
fi

echo "  ── --syscall filter by number ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --syscall 59 2>/dev/null)
if [[ "$output" == *"syscall=59"* ]]; then
    pass "--syscall 59 matches"
else
    fail "--syscall 59 — expected output, got: [$output]"
fi

# ── Arch filter ──────────────────────────────────────────────────────────────

echo "  ── --arch filter matches x86_64 ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --arch c000003e 2>/dev/null)
if [[ "$output" == *"arch=c000003e"* ]]; then
    pass "--arch c000003e matches"
else
    fail "--arch — expected output, got: [$output]"
fi

# ── Success filter ────────────────────────────────────────────────────────────

echo "  ── --success filter matches yes ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --success yes 2>/dev/null)
if [[ "$output" == *"success=yes"* ]] && [[ "$output" != *"success=no"* ]]; then
    pass "--success yes matches"
else
    fail "--success yes — unexpected output: [$output]"
fi

echo "  ── --success filter matches no ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --success no 2>/dev/null)
if [[ "$output" == *"success=no"* ]] && [[ "$output" != *"success=yes"* ]]; then
    pass "--success no matches"
else
    fail "--success no — unexpected output: [$output]"
fi

# ── Exit code filter ──────────────────────────────────────────────────────────

echo "  ── -e filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -e 0 2>/dev/null)
if [[ "$output" == *"exit=0"* ]]; then
    pass "-e 0 matches"
else
    fail "-e 0 — expected output, got: [$output]"
fi

echo "  ── -e filter excludes ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -e 999 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "-e 999 excluded"
else
    fail "-e 999 — expected no output, got: [$output]"
fi

# ── File/Key filter ─────────────────────────────────────────────────────────

echo "  ── -f filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -f secret 2>/dev/null)
if [[ "$output" == *"name=\"secret\""* ]]; then
    pass "-f secret matches PATH name"
else
    fail "-f secret — expected output, got: [$output]"
fi

echo "  ── -k filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -k access 2>/dev/null)
if [[ "$output" == *"key=\"access\""* ]]; then
    pass "-k access matches"
else
    fail "-k access — expected output, got: [$output]"
fi

# ── SELinux filters ──────────────────────────────────────────────────────────

echo "  ── --subject filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --subject unconfined_t 2>/dev/null)
if [[ "$output" == *"scontext=unconfined_u:unconfined_r:unconfined_t"* ]]; then
    pass "--subject matches"
else
    fail "--subject — expected output, got: [$output]"
fi

echo "  ── -o filter matches ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -o shadow_t 2>/dev/null)
if [[ "$output" == *"tcontext=system_u:object_r:shadow_t"* ]]; then
    pass "-o shadow_t matches"
else
    fail "-o — expected output, got: [$output]"
fi

echo "  ── --context filter matches subject ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --context unconfined_t 2>/dev/null)
if [[ "$output" == *"scontext=unconfined_u:unconfined_r:unconfined_t"* ]]; then
    pass "--context matches subject"
else
    fail "--context subject — expected output, got: [$output]"
fi

echo "  ── --context filter matches object ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --context shadow_t 2>/dev/null)
if [[ "$output" == *"tcontext=system_u:object_r:shadow_t"* ]]; then
    pass "--context matches object"
else
    fail "--context object — expected output, got: [$output]"
fi

# ── Time filters ──────────────────────────────────────────────────────────────

echo "  ── --start filter (numeric) ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --start 1680000000 2>/dev/null)
if [[ -n "$output" ]]; then
    pass "--start 1680000000 produced output"
else
    fail "--start — expected output, got none"
fi

echo "  ── --end filter (numeric) ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --end 1680000001 2>/dev/null)
if [[ -n "$output" ]] && [[ "$output" != *"1680000002"* ]]; then
    pass "--end 1680000001 excludes later events"
else
    fail "--end — unexpected output: [$output]"
fi

echo "  ── time range filtering ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --start 1680000000 --end 1680000000 2>/dev/null)
if [[ "$output" == *"1680000000"* ]] && [[ "$output" != *"1680000001"* ]] && [[ "$output" != *"1680000002"* ]]; then
    pass "time range keeps only events in [start, end]"
else
    fail "time range — unexpected output: [$output]"
fi

# ── Output formats ────────────────────────────────────────────────────────────

echo "  ── default output format ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -m SYSCALL 2>/dev/null)
if [[ "$output" == *"---- time->"* ]] && [[ "$output" == *"type=SYSCALL"* ]]; then
    pass "default output format includes headers"
else
    fail "default output format — expected headers, got: [$output]"
fi

echo "  ── raw output format ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch -r -m SYSCALL 2>/dev/null)
if [[ "$output" == *"type=SYSCALL msg=audit(1680000000.123:456):"* ]] && [[ "$output" != *"---- time->"* ]]; then
    pass "raw output prints original lines without headers"
else
    fail "raw output — unexpected: [$output]"
fi

echo "  ── interpret mode resolves uid and syscall ──"
output=$(echo 'type=SYSCALL msg=audit(1680000000.123:456): arch=c000003e syscall=2 success=yes exit=0 uid=0 comm="cat"' | $MODBOX ausearch --interpret 2>/dev/null)
if [[ "$output" == *"syscall=open"* ]] && [[ "$output" == *"uid=root"* ]]; then
    pass "interpret resolves syscall 2→open and uid 0→root"
else
    fail "interpret — expected syscall=open uid=root, got: [$output]"
fi

echo "  ── interpret mode via --format ──"
output=$(echo 'type=SYSCALL msg=audit(1680000000.123:456): arch=c000003e syscall=59 success=yes exit=0 uid=0 comm="cat"' | $MODBOX ausearch --format interpret 2>/dev/null)
if [[ "$output" == *"syscall=execve"* ]]; then
    pass "--format interpret resolves execve"
else
    fail "--format interpret — expected syscall=execve, got: [$output]"
fi

echo "  ── just-one flag ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --just-one 2>/dev/null)
count=$(printf '%s\n' "$output" | grep -c '^---- time->' || true)
if [[ "$count" -le 1 ]]; then
    pass "just-one flag limits output (got $count events)"
else
    fail "just-one flag — expected <=1 event, got $count"
fi

# ── Word matching ─────────────────────────────────────────────────────────────

echo "  ── word matching (--word) ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --word -c ls 2>/dev/null)
if [[ "$output" == *"comm=\"ls\""* ]]; then
    pass "word matching works for 'ls'"
else
    fail "word matching — expected output for 'ls', got: [$output]"
fi

# ── Combined filters ──────────────────────────────────────────────────────────

echo "  ── combined --uid + -c filter ──"
output=$(echo "$AUDIT_SAMPLE" | $MODBOX ausearch --uid 1000 -c ls 2>/dev/null)
if [[ "$output" == *"uid=1000"* ]] && [[ "$output" == *"comm=\"ls\""* ]]; then
    pass "combined --uid + -c filter works"
else
    fail "combined filter — expected output, got: [$output]"
fi

# ── Input from file ───────────────────────────────────────────────────────────

echo "  ── input from file ──"
echo "$AUDIT_SAMPLE" > "$TMPDIR/audit.log"
output=$($MODBOX ausearch -i "$TMPDIR/audit.log" -m SYSCALL 2>/dev/null)
if [[ "$output" == *"type=SYSCALL"* ]]; then
    pass "input from file works"
else
    fail "input from file — expected output, got: [$output]"
fi

# ── Unsupported libaudit flags ────────────────────────────────────────────────

echo "  ── libaudit flags rejected ──"
assert_cmd_pat_stderr 'libaudit' ausearch -a 100
assert_cmd_pat_stderr 'libaudit' ausearch -b
assert_cmd_pat_stderr 'libaudit' ausearch --lastreload

# ── Exit codes ────────────────────────────────────────────────────────────────

echo "  ── exit codes ──"
$MODBOX ausearch --help >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "--help exits 0"; else fail "--help exit code"; fi

$MODBOX ausearch --version >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "--version exits 0"; else fail "--version exit code"; fi

$MODBOX ausearch --foo >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "unknown option exits non-zero"; else fail "unknown option should exit non-zero"; fi

echo "" | $MODBOX ausearch >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "empty input exits 0"; else fail "empty input should exit 0"; fi

echo "  ── all tests completed ──"