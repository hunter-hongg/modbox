#!/usr/bin/env bash
#
# test_audit2allow.sh — Tests for audit2allow command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# Shared AVC denial lines for tests
AVC_FULL='type=AVC msg=audit(1680000000.000:123): avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0'
AVC_SHORT='avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0'
AVC_MULTI='avc:  denied  { read write } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0'

# Helper: run modbox audit2allow with stdin piped
# Usage: with_input "data" [args...]
with_input() {
    local data="$1"; shift
    echo "$data" | $MODBOX audit2allow "$@"
}

echo ""
echo "── audit2allow ────────────────────────────────────"

# ── Help & Version ───────────────────────────────────────────────────────────

echo "  ── --help ──"
assert_cmd_pat 'Usage: audit2allow' audit2allow --help

echo "  ── --version ──"
assert_cmd_pat 'audit2allow \(modbox\) 1\.0' audit2allow --version

# ── Error handling ───────────────────────────────────────────────────────────

echo "  ── unknown option rejected ──"
assert_cmd_pat_stderr 'unrecognized option' audit2allow --foo

echo "  ── conflicting input sources ──"
assert_cmd_pat_stderr 'conflicting input sources' audit2allow -a -i /dev/null
assert_cmd_pat_stderr 'conflicting input sources' audit2allow -b -i /dev/null
assert_cmd_pat_stderr 'conflicting input sources' audit2allow -d -a

echo "  ── unsupported flags ──"
assert_cmd_pat_stderr 'libaudit' audit2allow -a
assert_cmd_pat_stderr 'libaudit' audit2allow -b
assert_cmd_pat_stderr 'libaudit' audit2allow -l
assert_cmd_pat_stderr 'not supported' audit2allow -M test
assert_cmd_pat_stderr 'not supported' audit2allow -C

echo "  ── conflicting -M ──"
assert_cmd_pat_stderr 'conflicts with' audit2allow -M test -o /tmp/x

echo "  ── no input on terminal ──"
if [[ -t 0 ]]; then
    assert_cmd_pat_stderr 'no input specified' audit2allow
else
    echo "  SKIP — stdin is not a terminal in this context"
fi

# ── Empty input ──────────────────────────────────────────────────────────────

echo "  ── empty input ──"
output=$(echo "" | audit2allow 2>/dev/null)
if [[ -z "$output" ]]; then
    pass "empty input produces no output"
else
    fail "empty input — expected no output, got [$output]"
fi

# ── Basic AVC parsing and traditional output ─────────────────────────────────

echo "  ── basic AVC parsing (short format) ──"
output=$(with_input "$AVC_SHORT")
if [[ "$output" == *"#============= init_t =============="* ]] && \
   [[ "$output" == *"allow init_t passwd_file_t:file read;"* ]]; then
    pass "basic AVC parsing (short format)"
else
    fail "basic AVC parsing — unexpected output: [$output]"
fi

echo "  ── basic AVC parsing (full format) ──"
output=$(with_input "$AVC_FULL")
if [[ "$output" == *"#============= init_t =============="* ]] && \
   [[ "$output" == *"allow init_t passwd_file_t:file read;"* ]]; then
    pass "basic AVC parsing (full format)"
else
    fail "basic AVC parsing (full) — unexpected output: [$output]"
fi

echo "  ── multiple perms ──"
output=$(with_input "$AVC_MULTI")
if [[ "$output" == *"{ read write }"* ]]; then
    pass "multiple perms grouped"
else
    fail "multiple perms — unexpected output: [$output]"
fi

echo "  ── multiple different denials ──"
cat > "$TMPDIR/multi.avc" <<'AVCEOF'
avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
avc:  denied  { write } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
AVCEOF
output=$($MODBOX audit2allow -i "$TMPDIR/multi.avc")
if [[ "$output" == *"{ read write }"* ]]; then
    pass "multiple denials grouped into single rule"
else
    fail "multiple denials — unexpected output: [$output]"
fi

# ── Deduplication ────────────────────────────────────────────────────────────

echo "  ── deduplication ──"
cat > "$TMPDIR/dedup.avc" <<'AVCEOF'
avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
AVCEOF
output=$($MODBOX audit2allow -i "$TMPDIR/dedup.avc")
count=$(printf '%s\n' "$output" | grep -c '^allow ')
if [[ "$count" -eq 1 ]]; then
    pass "deduplication: identical denials produce one rule"
else
    fail "deduplication — expected 1 allow rule, got $count"
fi

# ── Module output ────────────────────────────────────────────────────────────

echo "  ── module output ──"
output=$(with_input "$AVC_SHORT" -m testmod)
if [[ "$output" == *"module testmod 1.0;"* ]] && \
   [[ "$output" == *"require {"* ]] && \
   [[ "$output" == *"allow init_t passwd_file_t:file read;"* ]]; then
    pass "module output format"
else
    fail "module output — unexpected: [$output]"
fi

echo "  ── module with multiple perms ──"
output=$(with_input "$AVC_MULTI" -m testmod)
if [[ "$output" == *"class file { read write }"* ]]; then
    pass "module require block groups perms"
else
    fail "module require perms — unexpected: [$output]"
fi

echo "  ── module with dedup ──"
output=$(cat "$TMPDIR/dedup.avc" | $MODBOX audit2allow -m testmod)
count=$(printf '%s\n' "$output" | grep -c '^allow ')
if [[ "$count" -eq 1 ]]; then
    pass "module output deduplication"
else
    fail "module dedup — expected 1 allow rule, got $count"
fi

# ── Require-only output ──────────────────────────────────────────────────────

echo "  ── require-only output ──"
output=$(with_input "$AVC_SHORT" -r)
if [[ "$output" == *"require {"* ]] && \
   [[ "$output" == *"allow init_t passwd_file_t:file read;"* ]]; then
    pass "require-only output"
else
    fail "require-only — unexpected: [$output]"
fi

# ── Dontaudit ────────────────────────────────────────────────────────────────

echo "  ── dontaudit output ──"
output=$(with_input "$AVC_SHORT" -D)
if [[ "$output" == *"dontaudit init_t passwd_file_t:file read;"* ]] && \
   [[ "$output" != *"allow init_t passwd_file_t:file read;"* ]]; then
    pass "dontaudit output"
else
    fail "dontaudit — unexpected: [$output]"
fi

echo "  ── dontaudit with module ──"
output=$(with_input "$AVC_SHORT" -D -m testmod)
if [[ "$output" == *"dontaudit init_t passwd_file_t:file read;"* ]]; then
    pass "dontaudit in module"
else
    fail "dontaudit module — unexpected: [$output]"
fi

# ── Type filter ──────────────────────────────────────────────────────────────

echo "  ── type filter matches ──"
output=$(with_input "$AVC_FULL" -t "AVC")
if [[ -n "$output" ]]; then
    pass "type filter matches AVC"
else
    fail "type filter — expected output, got none"
fi

echo "  ── type filter excludes ──"
output=$(with_input "$AVC_FULL" -t "SYSCALL")
if [[ -z "$output" ]]; then
    pass "type filter excludes SYSCALL"
else
    fail "type filter — expected no output, got: [$output]"
fi

# ── Why mode ─────────────────────────────────────────────────────────────────

echo "  ── why output ──"
output=$(with_input "$AVC_SHORT" -w)
if [[ "$output" == *"comm=systemd"* ]] && \
   [[ "$output" == *"source system_u:system_r:init_t:s0"* ]] && \
   [[ "$output" == *"target system_u:object_r:passwd_file_t:s0"* ]]; then
    pass "why output format"
else
    fail "why output — unexpected: [$output]"
fi

# ── Output to file ───────────────────────────────────────────────────────────

echo "  ── output to file ──"
echo "$AVC_SHORT" | $MODBOX audit2allow -o "$TMPDIR/out.te"
if [[ -f "$TMPDIR/out.te" ]] && grep -q "allow init_t passwd_file_t:file read;" "$TMPDIR/out.te"; then
    pass "output to file"
else
    fail "output to file — content not written correctly"
fi

echo "  ── output file append ──"
echo "$AVC_SHORT" | $MODBOX audit2allow -o "$TMPDIR/out.te"
lines=$(grep -c '^allow' "$TMPDIR/out.te")
if [[ "$lines" -eq 2 ]]; then
    pass "output file append"
else
    fail "output append — expected 2 rules, got $lines"
fi

# ── Non-AVC lines skipped ────────────────────────────────────────────────────

echo "  ── non-AVC lines skipped ──"
cat > "$TMPDIR/mixed.avc" <<'AVCEOF'
type=SYSCALL msg=audit(1680000000.000:123): arch=c000003e syscall=2 success=no
this is not an audit line
avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
AVCEOF
output=$($MODBOX audit2allow -i "$TMPDIR/mixed.avc")
if [[ "$output" == *"allow init_t passwd_file_t:file read;"* ]]; then
    pass "non-AVC lines skipped"
else
    fail "non-AVC skip — unexpected: [$output]"
fi

# ── Warning flags ────────────────────────────────────────────────────────────

echo "  ── warning flags ──"
# -R: warn on stderr, still produce rules on stdout
output=$(with_input "$AVC_SHORT" -R 2>/dev/null)
if [[ "$output" == *"allow init_t"* ]]; then
    pass "warning flag -R still produces rules"
else
    fail "warning flag -R — unexpected output: [$output]"
fi
# -x: warn on stderr, normal rules still produced (xperms not supported in v1)
output=$(with_input "$AVC_SHORT" -x 2>/dev/null)
if [[ "$output" == *"allow init_t"* ]]; then
    pass "warning flag -x ignores xperms but still produces normal rules"
else
    fail "warning flag -x — unexpected output: [$output]"
fi

# ── Exit codes ───────────────────────────────────────────────────────────────

echo "  ── exit codes ──"
$MODBOX audit2allow --help >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "--help exits 0"; else fail "--help exit code"; fi

$MODBOX audit2allow --version >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "--version exits 0"; else fail "--version exit code"; fi

$MODBOX audit2allow --foo >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "unknown option exits non-zero"; else fail "unknown option should exit non-zero"; fi

echo "" | $MODBOX audit2allow >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "empty input exits 0"; else fail "empty input should exit 0"; fi

# ── Full integration: compare with reference ────────────────────────────────

echo "  ── output comparison with reference (when available) ──"
if command -v /usr/sbin/audit2allow >/dev/null 2>&1; then
    ref_output=$(echo "$AVC_SHORT" | /usr/sbin/audit2allow 2>/dev/null)
    mod_output=$(echo "$AVC_SHORT" | $MODBOX audit2allow 2>/dev/null)

    if [[ "$mod_output" == *"allow init_t passwd_file_t:file read;"* ]]; then
        pass "output contains correct allow rule"
    else
        fail "output mismatch with reference — modbox: [$mod_output] ref: [$ref_output]"
    fi

    ref_mod=$(echo "$AVC_SHORT" | /usr/sbin/audit2allow -m testmod 2>/dev/null)
    mod_mod=$(echo "$AVC_SHORT" | $MODBOX audit2allow -m testmod 2>/dev/null)
    if [[ "$mod_mod" == *"module testmod 1.0;"* ]] && \
       [[ "$mod_mod" == *"allow init_t passwd_file_t:file read;"* ]]; then
        pass "module output format correct"
    else
        fail "module output mismatch — modbox: [$mod_mod] ref: [$ref_mod]"
    fi
else
    echo "  SKIP — /usr/sbin/audit2allow not available"
fi
