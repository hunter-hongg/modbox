SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── cmp ──────────────────────────────────────"

# Fixtures. cmp is byte-oriented, so build them with printf escapes to get
# exact control over line/byte offsets.
printf 'abc\ndef\n' > "$TMPDIR"/cmp_a
printf 'abc\ndef\n' > "$TMPDIR"/cmp_a_copy
printf 'abc\nxyz\n' > "$TMPDIR"/cmp_b
printf 'abc\n'       > "$TMPDIR"/cmp_short
printf ''            > "$TMPDIR"/cmp_empty
printf 'abcdef'      > "$TMPDIR"/cmp_noeol1
printf 'abcxef'      > "$TMPDIR"/cmp_noeol2
printf '\001\002'    > "$TMPDIR"/cmp_bin1
printf '\003\004'    > "$TMPDIR"/cmp_bin2

# ── exit status: identical / differ ─────────────────────────────────────────

echo "  ── exit status ──"

"$MODBOX" cmp "$TMPDIR"/cmp_a "$TMPDIR"/cmp_a_copy >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp identical files → exit 0"
else
    fail "cmp identical files — expected exit 0"
fi

"$MODBOX" cmp "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp differing files → exit 1"
else
    fail "cmp differing files — expected exit 1"
fi

"$MODBOX" cmp "$TMPDIR"/cmp_a "$TMPDIR"/cmp_nonexistent >/dev/null 2>&1
if [[ $? -eq 2 ]]; then
    pass "cmp missing file → exit 2"
else
    fail "cmp missing file — expected exit 2"
fi

"$MODBOX" cmp --bogus-option "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 2 ]]; then
    pass "cmp bad option → exit 2"
else
    fail "cmp bad option — expected exit 2"
fi

# ── default differ message ──────────────────────────────────────────────────

echo "  ── default output ──"

# The report goes to stdout: "FILE1 FILE2 differ: byte N, line M".
out=$("$MODBOX" cmp "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b 2>/dev/null || true)
if [[ "$out" == *"differ: byte 5, line 2"* ]]; then
    pass "cmp reports first difference (byte 5, line 2)"
else
    fail "cmp default output — expected 'differ: byte 5, line 2', got [$out]"
fi

# Identical files print nothing.
out=$("$MODBOX" cmp "$TMPDIR"/cmp_a "$TMPDIR"/cmp_a_copy 2>/dev/null || true)
if [[ -z "$out" ]]; then
    pass "cmp identical files print nothing"
else
    fail "cmp identical files — expected no output, got [$out]"
fi

# ── -s / --quiet / --silent ─────────────────────────────────────────────────

echo "  ── -s (silent) ──"

out=$("$MODBOX" cmp -s "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b 2>/dev/null || true)
if [[ -z "$out" ]]; then
    pass "cmp -s differing files print nothing"
else
    fail "cmp -s — expected no output, got [$out]"
fi

"$MODBOX" cmp -s "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp -s differing files → exit 1"
else
    fail "cmp -s — expected exit 1"
fi

"$MODBOX" cmp --quiet "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp --quiet → exit 1"
else
    fail "cmp --quiet — expected exit 1"
fi

out=$("$MODBOX" cmp --silent "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b 2>/dev/null || true)
if [[ -z "$out" ]]; then
    pass "cmp --silent print nothing"
else
    fail "cmp --silent — expected no output, got [$out]"
fi

# ── -l (list all differing bytes) ───────────────────────────────────────────

echo "  ── -l ──"

# Octal byte values: 100 → 144, 170 → 170 (GNU prints raw file octets).
out=$("$MODBOX" cmp -l "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b 2>/dev/null || true)
if [[ "$out" == *"5 144 170"* ]]; then
    pass "cmp -l reports octal byte values"
else
    fail "cmp -l — expected '5 144 170', got [$out]"
fi

# All differing bytes are listed, not just the first.
count=$(printf '%s\n' "$out" | grep -cE '^[0-9]+ ' || true)
if [[ "$count" -eq 3 ]]; then
    pass "cmp -l lists every differing byte (3)"
else
    fail "cmp -l — expected 3 lines, got $count"
fi

# Identical files with -l print nothing and exit 0.
"$MODBOX" cmp -l "$TMPDIR"/cmp_a "$TMPDIR"/cmp_a_copy >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp -l identical files → exit 0"
else
    fail "cmp -l identical — expected exit 0"
fi

# -l and -s are mutually exclusive → usage error (exit 2).
"$MODBOX" cmp -l -s "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 2 ]]; then
    pass "cmp -l -s conflict → exit 2"
else
    fail "cmp -l -s — expected exit 2"
fi
assert_cmd_pat_stderr 'incompatible' cmp -l -s "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b

# ── -b / --print-bytes ──────────────────────────────────────────────────────

echo "  ── -b ──"

out=$("$MODBOX" cmp -b "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b 2>/dev/null || true)
if [[ "$out" == *"differ: byte 5, line 2"* && "$out" == *"144 d 170 x"* ]]; then
    pass "cmp -b appends printable/octal byte detail"
else
    fail "cmp -b — expected byte detail '144 d 170 x', got [$out]"
fi

# ── EOF / prefix cases ──────────────────────────────────────────────────────

echo "  ── EOF handling ──"

# Shorter file: diagnostic on stderr, exit 1, nothing on stdout.
out=$("$MODBOX" cmp "$TMPDIR"/cmp_short "$TMPDIR"/cmp_a 2>/dev/null || true)
if [[ -z "$out" ]]; then
    pass "cmp prefix case prints nothing to stdout"
else
    fail "cmp prefix case — expected empty stdout, got [$out]"
fi
assert_cmd_pat_stderr 'EOF' cmp "$TMPDIR"/cmp_short "$TMPDIR"/cmp_a

"$MODBOX" cmp "$TMPDIR"/cmp_short "$TMPDIR"/cmp_a >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp prefix case → exit 1"
else
    fail "cmp prefix case — expected exit 1"
fi

# Empty file vs non-empty.
"$MODBOX" cmp "$TMPDIR"/cmp_empty "$TMPDIR"/cmp_a >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp empty vs non-empty → exit 1"
else
    fail "cmp empty vs non-empty — expected exit 1"
fi
assert_cmd_pat_stderr 'empty' cmp "$TMPDIR"/cmp_empty "$TMPDIR"/cmp_a

# -s suppresses the EOF diagnostic but keeps the exit status.
out=$("$MODBOX" cmp -s "$TMPDIR"/cmp_short "$TMPDIR"/cmp_a 2>&1 || true)
if [[ -z "$out" ]]; then
    pass "cmp -s suppresses the EOF diagnostic"
else
    fail "cmp -s prefix case — expected no output, got [$out]"
fi

# When the shorter input is a prefix but a real difference was already found
# earlier, that difference is the report: stdout gets the normal line and the
# EOF note is dropped, matching GNU cmp.
printf 'abcd\n' > "$TMPDIR"/cmp_prefix_long
printf 'abc\n'  > "$TMPDIR"/cmp_prefix_short
out=$("$MODBOX" cmp "$TMPDIR"/cmp_prefix_long "$TMPDIR"/cmp_prefix_short 2>/dev/null || true)
if [[ "$out" == *"differ: byte 4, line 1"* ]]; then
    pass "cmp reports an earlier difference at the prefix boundary on stdout"
else
    fail "cmp prefix-boundary difference — expected 'differ: byte 4, line 1', got [$out]"
fi
err=$("$MODBOX" cmp "$TMPDIR"/cmp_prefix_long "$TMPDIR"/cmp_prefix_short 2>&1 >/dev/null || true)
if [[ -z "$err" ]]; then
    pass "cmp drops the EOF note when a difference already described the boundary"
else
    fail "cmp prefix-boundary difference — expected no stderr, got [$err]"
fi

# When the only difference is the boundary itself, the report is the EOF note
# and stdout stays empty (GNU does not print a 'differ:' line for it).
printf 'ab'  > "$TMPDIR"/cmp_short_nl
printf 'abX' > "$TMPDIR"/cmp_long_x
out=$("$MODBOX" cmp "$TMPDIR"/cmp_long_x "$TMPDIR"/cmp_short_nl 2>/dev/null || true)
if [[ -z "$out" ]]; then
    pass "cmp prints no 'differ:' line when only the length differs"
else
    fail "cmp pure length difference — expected empty stdout, got [$out]"
fi
assert_cmd_pat_stderr 'in line 1' cmp "$TMPDIR"/cmp_long_x "$TMPDIR"/cmp_short_nl

# -l mode names the short input without line information.
err=$("$MODBOX" cmp -l "$TMPDIR"/cmp_prefix_long "$TMPDIR"/cmp_prefix_short 2>&1 >/dev/null || true)
if [[ "$err" == *"EOF on"*"after byte 4"* && "$err" != *"line"* ]]; then
    pass "cmp -l EOF note omits the line information"
else
    fail "cmp -l EOF note — expected 'after byte 4' without line info, got [$err]"
fi

# ── -n / --bytes=LIMIT ──────────────────────────────────────────────────────

echo "  ── -n ──"

# Differences past the limit are ignored.
"$MODBOX" cmp -n 2 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp -n 2 only compares the first 2 bytes → exit 0"
else
    fail "cmp -n 2 — expected exit 0"
fi

# A limit of 0 compares nothing.
"$MODBOX" cmp -n 0 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp -n 0 → exit 0"
else
    fail "cmp -n 0 — expected exit 0"
fi

# A limit past the first difference still reports it.
"$MODBOX" cmp --bytes=10 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp --bytes=10 → exit 1"
else
    fail "cmp --bytes=10 — expected exit 1"
fi

# ── -i / --ignore-initial=SKIP ──────────────────────────────────────────────

echo "  ── --ignore-initial ──"

# Skipping 4 bytes lands on the first differing byte, reported as byte 1.
out=$("$MODBOX" cmp --ignore-initial=4 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b 2>/dev/null || true)
if [[ "$out" == *"byte 1, line 1"* ]]; then
    pass "cmp --ignore-initial=4 renumbers the reported byte"
else
    fail "cmp --ignore-initial=4 — expected 'byte 1, line 1', got [$out]"
fi

# Skipping past every difference makes the inputs compare equal. cmp_a/cmp_b
# share only the trailing newline at byte 8, so skipping 7 bytes leaves that.
"$MODBOX" cmp -i 7 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp -i 7 skips every difference → exit 0"
else
    fail "cmp -i 7 — expected exit 0"
fi

# Both explicit forms produce identical results.
"$MODBOX" cmp --ignore-initial=4 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
rc_long=$?
"$MODBOX" cmp -i 4 "$TMPDIR"/cmp_a "$TMPDIR"/cmp_b >/dev/null 2>&1
rc_short=$?
if [[ "$rc_long" -eq "$rc_short" ]]; then
    pass "cmp -i and --ignore-initial agree"
else
    fail "cmp -i / --ignore-initial disagree ($rc_short vs $rc_long)"
fi

# ── stdin support ───────────────────────────────────────────────────────────

echo "  ── stdin ──"

# A single operand reads the other side from stdin.
printf 'abc\nxyz\n' | "$MODBOX" cmp "$TMPDIR"/cmp_a - >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp FILE - reads stdin"
else
    fail "cmp FILE - — expected exit 1"
fi

printf 'abc\ndef\n' | "$MODBOX" cmp "$TMPDIR"/cmp_a - >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp FILE - matching stdin → exit 0"
else
    fail "cmp FILE - matching — expected exit 0"
fi

# Two operands, first is stdin.
printf 'abc\nxyz\n' | "$MODBOX" cmp - "$TMPDIR"/cmp_a >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp - FILE reads stdin"
else
    fail "cmp - FILE — expected exit 1"
fi

# Only one operand given: second defaults to stdin.
printf 'abc\ndef\n' | "$MODBOX" cmp "$TMPDIR"/cmp_a >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp FILE defaults the second operand to stdin"
else
    fail "cmp FILE (stdin default) — expected exit 0"
fi

# ── binary-safe comparison ──────────────────────────────────────────────────

echo "  ── binary safety ──"

"$MODBOX" cmp "$TMPDIR"/cmp_bin1 "$TMPDIR"/cmp_bin2 >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp compares NUL/non-printable bytes"
else
    fail "cmp binary — expected exit 1"
fi

out=$("$MODBOX" cmp -l "$TMPDIR"/cmp_bin1 "$TMPDIR"/cmp_bin2 2>/dev/null || true)
if [[ "$out" == *"1   1   3"* && "$out" == *"2   2   4"* ]]; then
    pass "cmp -l reports raw octal values for binary data"
else
    fail "cmp -l binary — expected '1   1   3' and '2   2   4', got [$out]"
fi

# A final line without a trailing newline must not be treated as different.
"$MODBOX" cmp "$TMPDIR"/cmp_noeol1 "$TMPDIR"/cmp_noeol1 >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp tolerates a missing trailing newline"
else
    fail "cmp missing trailing newline — expected exit 0"
fi

"$MODBOX" cmp "$TMPDIR"/cmp_noeol1 "$TMPDIR"/cmp_noeol2 >/dev/null 2>&1
if [[ $? -eq 1 ]]; then
    pass "cmp detects a difference inside a newline-less final line"
else
    fail "cmp newline-less difference — expected exit 1"
fi

# ── identical arguments / empty vs empty ────────────────────────────────────

echo "  ── degenerate inputs ──"

"$MODBOX" cmp "$TMPDIR"/cmp_a "$TMPDIR"/cmp_a >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp same file twice → exit 0"
else
    fail "cmp same file twice — expected exit 0"
fi

"$MODBOX" cmp "$TMPDIR"/cmp_empty "$TMPDIR"/cmp_empty >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp empty vs empty → exit 0"
else
    fail "cmp empty vs empty — expected exit 0"
fi

# ── help / version ──────────────────────────────────────────────────────────

echo "  ── help and version ──"

assert_cmd_pat 'Usage:' cmp --help
assert_cmd_pat 'differ' cmp --help

"$MODBOX" cmp --help >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp --help → exit 0"
else
    fail "cmp --help — expected exit 0"
fi

assert_cmd_pat 'cmp' cmp --version
"$MODBOX" cmp --version >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
    pass "cmp --version → exit 0"
else
    fail "cmp --version — expected exit 0"
fi
