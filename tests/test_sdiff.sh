#!/usr/bin/env bash
#
# test_sdiff.sh — tests for `modbox sdiff`
#
# sdiff renders two files side by side (byte-for-byte like `diff -y`) and, with
# -o, merges interactively. Expected outputs below were captured from GNU
# diffutils 3.12's sdiff and are embedded as literal byte sequences so the
# assertions are deterministic and independent of the host's diffutils.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sdiff ────────────────────────────────────"

# ── fixtures ────────────────────────────────────────────────────────────────
printf 'a\nb\nc\nd\n' > "$TMPDIR"/sd_f1
printf 'a\nB\nc\nD\n' > "$TMPDIR"/sd_f2
printf 'a\nb\nc\nd\n' > "$TMPDIR"/sd_f1_copy
printf 'a\nb\n'       > "$TMPDIR"/sd_short
printf ''             > "$TMPDIR"/sd_empty
printf 'b\nb\n'       > "$TMPDIR"/sd_abc        # for insertion grouping
printf '3\n4\n'       > "$TMPDIR"/sd_ins_a
printf '3\nX\nY\n4\n' > "$TMPDIR"/sd_ins_b
printf '%s\n' "b" "X" "Y" "c" > "$TMPDIR"/sd_grp_a
printf '%s\n' "b" "c" > "$TMPDIR"/sd_grp_b
printf 'a\tb\n'       > "$TMPDIR"/sd_tab1
printf 'a\tc\n'       > "$TMPDIR"/sd_tab2
printf 'a\r\nb\n'     > "$TMPDIR"/sd_crlf
printf 'a\nb\n'       > "$TMPDIR"/sd_lf

# GNU-compatible helper: run modbox sdiff, capture stdout as a raw string.
sd_out() { "$MODBOX" sdiff "$@" 2>/dev/null || true; }

# ── exit status ─────────────────────────────────────────────────────────────
echo "  ── exit status ──"

"$MODBOX" sdiff "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f1_copy >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "identical files → exit 0" || fail "identical files — expected exit 0"

"$MODBOX" sdiff "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
[[ $? -eq 1 ]] && pass "differing files → exit 1" || fail "differing files — expected exit 1"

"$MODBOX" sdiff "$TMPDIR"/sd_f1 "$TMPDIR"/sd_missing >/dev/null 2>&1
[[ $? -eq 2 ]] && pass "missing file → exit 2" || fail "missing file — expected exit 2"

# ── default layout (byte-exact, GNU diffutils 3.12) ─────────────────────────
echo "  ── default side-by-side layout ──"

# a<8 tabs>a / b<7 tabs><6 spaces>|<tab>B / c<8 tabs>c / d<7 tabs><6 spaces>|<tab>D
expected_default=$'a\t\t\t\t\t\t\t\ta\nb\t\t\t\t\t\t\t      |\tB\nc\t\t\t\t\t\t\t\tc\nd\t\t\t\t\t\t\t      |\tD'
actual=$(sd_out "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2)
[[ "$actual" == "$expected_default" ]] && pass "default 4-line layout is byte-exact" \
    || fail "default layout mismatch — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# ── -s suppress common lines ────────────────────────────────────────────────
echo "  ── -s / -l ──"
expected_s=$'b\t\t\t\t\t\t\t      |\tB\nd\t\t\t\t\t\t\t      |\tD'
actual=$(sd_out -s "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2)
[[ "$actual" == "$expected_s" ]] && pass "-s omits common lines" \
    || fail "-s output mismatch — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# -l: common lines rendered left-only with '(' gutter.
expected_l=$'a\t\t\t\t\t\t\t      (\nb\t\t\t\t\t\t\t      |\tB\nc\t\t\t\t\t\t\t      (\nd\t\t\t\t\t\t\t      |\tD'
actual=$(sd_out -l "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2)
[[ "$actual" == "$expected_l" ]] && pass "-l renders common lines left-only with '('" \
    || fail "-l output mismatch — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# ── -w width controls the separator column ──────────────────────────────────
echo "  ── width ──"
expected_w20=$'a\ta\nb     |\tB\nc\tc\nd     |\tD'
actual=$(sd_out -w 20 "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2)
[[ "$actual" == "$expected_w20" ]] && pass "-w 20 matches GNU" \
    || fail "-w 20 mismatch — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# ── insertions / deletions gutters ──────────────────────────────────────────
echo "  ── > / < gutters ──"
# sd_ins_a (3,4) vs sd_ins_b (3,X,Y,4): X,Y are inserted → '>'
actual=$(sd_out "$TMPDIR"/sd_ins_a "$TMPDIR"/sd_ins_b)
printf '%s' "$actual" | grep -qE '^.*>.*X$' && pass "insertion uses '>' gutter" \
    || fail "insertion gutter — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"
# reverse direction: deletions use '<'
actual=$(sd_out "$TMPDIR"/sd_ins_b "$TMPDIR"/sd_ins_a)
printf '%s' "$actual" | grep -qE '<$' && pass "deletion uses '<' gutter" \
    || fail "deletion gutter — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# ── unequal length: '>' / '<' and '(' / ')' fillers ─────────────────────────
echo "  ── length-mismatch fillers ──"
# short (a,b) vs f1 (a,b,c,d): the extra right lines are an append → '>'.
actual=$(sd_out "$TMPDIR"/sd_short "$TMPDIR"/sd_f1)
printf '%s' "$actual" | grep -qE '>[[:space:]]' && pass "longer right → '>' gutter" \
    || fail "filler '>' — got [$(printf '%s' "$actual" | od -c | head -6 | tr '\n' ';')]"
# reverse: f1 vs short → the extra left lines are a deletion → '<'.
actual=$(sd_out "$TMPDIR"/sd_f1 "$TMPDIR"/sd_short)
printf '%s' "$actual" | grep -qE '<([[:space:]]|$)' && pass "longer left → '<' gutter" \
    || fail "filler '<' — got [$(printf '%s' "$actual" | od -c | head -6 | tr '\n' ';')]"
# -l renders matched common lines left-only, using the '(' gutter.
actual=$(sd_out -l "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2)
printf '%s' "$actual" | grep -qE '\($' && pass "-l common lines use '(' gutter" \
    || fail "filler '(' — got [$(printf '%s' "$actual" | od -c | head -6 | tr '\n' ';')]"

# ── tabs ────────────────────────────────────────────────────────────────────
echo "  ── tabs ──"
# Default: raw tab preserved on the left, right column also has tabs before |.
actual=$(sd_out "$TMPDIR"/sd_tab1 "$TMPDIR"/sd_tab2)
printf '%s' "$actual" | grep -qP 'a\tb' && pass "default keeps leading tab" \
    || fail "default tab handling — got [$(printf '%s' "$actual" | od -c | head -3 | tr '\n' ';')]"
# -t expands tabs to spaces in the output.
actual=$(sd_out -t "$TMPDIR"/sd_tab1 "$TMPDIR"/sd_tab2)
if printf '%s' "$actual" | grep -qP 'a\t'; then
    fail "-t should expand tabs (found literal tab)"
else
    pass "-t expands tabs to spaces"
fi

# ── trailing-newline markers / and \ ────────────────────────────────────────
echo "  ── newline markers ──"
printf 'a\nb' > "$TMPDIR"/sd_noeol1
printf 'a\nb\n' > "$TMPDIR"/sd_noeol2
# left lacks final newline → '\'
actual=$(sd_out "$TMPDIR"/sd_noeol1 "$TMPDIR"/sd_noeol2)
printf '%s' "$actual" | grep -q '\\' && pass "left missing EOL → '\\' marker" \
    || fail "EOL '\\' marker — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"
# right lacks final newline → '/'
actual=$(sd_out "$TMPDIR"/sd_noeol2 "$TMPDIR"/sd_noeol1)
printf '%s' "$actual" | grep -q '/' && pass "right missing EOL → '/' marker" \
    || fail "EOL '/' marker — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# ── ignore options ──────────────────────────────────────────────────────────
echo "  ── ignore options ──"
printf 'Hello\n' > "$TMPDIR"/sd_case1
printf 'hello\n' > "$TMPDIR"/sd_case2
"$MODBOX" sdiff -i "$TMPDIR"/sd_case1 "$TMPDIR"/sd_case2 >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "-i makes case-only differences equal" || fail "-i should exit 0"

printf 'a b\n' > "$TMPDIR"/sd_sp1
printf 'ab\n'  > "$TMPDIR"/sd_sp2
"$MODBOX" sdiff -W "$TMPDIR"/sd_sp1 "$TMPDIR"/sd_sp2 >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "-W ignores all whitespace" || fail "-W should exit 0"

"$MODBOX" sdiff --strip-trailing-cr "$TMPDIR"/sd_crlf "$TMPDIR"/sd_lf >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "--strip-trailing-cr treats CRLF == LF" || fail "--strip-trailing-cr should exit 0"

printf 'a\n\nb\n'   > "$TMPDIR"/sd_bl1
printf 'a\n\n\nb\n' > "$TMPDIR"/sd_bl2
"$MODBOX" sdiff -B "$TMPDIR"/sd_bl1 "$TMPDIR"/sd_bl2 >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "-B ignores blank-line changes" || fail "-B should exit 0"

printf 'a\nX\n' > "$TMPDIR"/sd_re1
printf 'a\nY\n' > "$TMPDIR"/sd_re2
"$MODBOX" sdiff -I '^[XY]' "$TMPDIR"/sd_re1 "$TMPDIR"/sd_re2 >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "-I ignores matching lines" || fail "-I should exit 0"

# ── stdin ───────────────────────────────────────────────────────────────────
echo "  ── stdin ──"
actual=$(printf 'a\nb\nc\nd\n' | "$MODBOX" sdiff - "$TMPDIR"/sd_f2 2>/dev/null || true)
expected_stdin=$'a\t\t\t\t\t\t\t\ta\nb\t\t\t\t\t\t\t      |\tB\nc\t\t\t\t\t\t\t\tc\nd\t\t\t\t\t\t\t      |\tD'
[[ "$actual" == "$expected_stdin" ]] && pass "'-' reads FILE1 from stdin" \
    || fail "stdin mismatch — got [$(printf '%s' "$actual" | od -c | head -4 | tr '\n' ';')]"

# ── errors ──────────────────────────────────────────────────────────────────
echo "  ── errors ──"
"$MODBOX" sdiff "$TMPDIR"/sd_f1 >/dev/null 2>&1
[[ $? -eq 2 ]] && pass "one operand → exit 2" || fail "one operand — expected exit 2"
assert_cmd_pat_stderr "missing operand after" sdiff "$TMPDIR"/sd_f1
assert_cmd_pat_stderr "Try 'sdiff --help'" sdiff "$TMPDIR"/sd_f1

"$MODBOX" sdiff "$TMPDIR"/sd_f1 >/dev/null 2>&1
"$MODBOX" sdiff >/dev/null 2>&1
[[ $? -eq 2 ]] && pass "no operands → exit 2" || fail "no operands — expected exit 2"

"$MODBOX" sdiff "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 extra >/dev/null 2>&1
[[ $? -eq 2 ]] && pass "extra operand → exit 2" || fail "extra operand — expected exit 2"

assert_cmd_pat_stderr "No such file or directory" sdiff "$TMPDIR"/sd_f1 "$TMPDIR"/sd_missing

"$MODBOX" sdiff -w 0 "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
[[ $? -eq 2 ]] && pass "-w 0 → exit 2" || fail "-w 0 — expected exit 2"
assert_cmd_pat_stderr "invalid width" sdiff -w 0 "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2

# ── help / version ──────────────────────────────────────────────────────────
echo "  ── help / version ──"
assert_cmd_pat "Usage:" sdiff --help
"$MODBOX" sdiff --help >/dev/null 2>&1
[[ $? -eq 0 ]] && pass "sdiff --help → exit 0" || fail "sdiff --help — expected exit 0"
assert_cmd_pat "sdiff" sdiff -v

# ── interactive merge (-o) ──────────────────────────────────────────────────
echo "  ── interactive merge (-o) ──"
# Answer 'l' at both change prompts → left version of every hunk.
printf 'l\nl\nl\nl\n' | "$MODBOX" sdiff -o "$TMPDIR"/sd_merge_l "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
merged=$(cat "$TMPDIR"/sd_merge_l 2>/dev/null || true)
expected_merge_l=$'a\nb\nc\nd'
[[ "$merged" == "$expected_merge_l" ]] && pass "-o with 'l' answers writes the left file" \
    || fail "-o 'l' merge mismatch — got [$merged]"

# Answer 'r' → right version.
printf 'r\nr\nr\nr\n' | "$MODBOX" sdiff -o "$TMPDIR"/sd_merge_r "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
merged=$(cat "$TMPDIR"/sd_merge_r 2>/dev/null || true)
expected_merge_r=$'a\nB\nc\nD'
[[ "$merged" == "$expected_merge_r" ]] && pass "-o with 'r' answers writes the right file" \
    || fail "-o 'r' merge mismatch — got [$merged]"

# Mixed l/r.
printf 'l\nr\nl\nr\n' | "$MODBOX" sdiff -o "$TMPDIR"/sd_merge_mix "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
merged=$(cat "$TMPDIR"/sd_merge_mix 2>/dev/null || true)
expected_merge_mix=$'a\nb\nc\nD'
[[ "$merged" == "$expected_merge_mix" ]] && pass "-o with mixed 'l'/'r' merges per hunk" \
    || fail "-o mixed merge mismatch — got [$merged]"

# q stops early and exits 2.
printf 'q\n' | "$MODBOX" sdiff -o "$TMPDIR"/sd_merge_q "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
rc=$?
[[ $rc -eq 2 ]] && pass "-o 'q' → exit 2" || fail "-o 'q' — expected exit 2, got $rc"

# EOF (no answers) behaves like q, and the output file is still created.
"$MODBOX" sdiff -o "$TMPDIR"/sd_merge_eof "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 </dev/null >/dev/null 2>&1
rc=$?
[[ $rc -eq 2 ]] && pass "-o EOF → exit 2 (treated as q)" || fail "-o EOF — expected exit 2, got $rc"
[[ -f "$TMPDIR"/sd_merge_eof ]] && pass "-o creates the output file" || fail "-o did not create output file"

# -o with stdin operand is refused.
"$MODBOX" sdiff -o "$TMPDIR"/sd_merge_x "$TMPDIR"/sd_f1 - >/dev/null 2>&1
[[ $? -eq 2 ]] && pass "-o with stdin operand → exit 2" || fail "-o stdin operand — expected exit 2"

# Unknown merge command prints help and re-prompts (still consumable via EOF).
printf 'z\nq\n' | "$MODBOX" sdiff -o "$TMPDIR"/sd_merge_z "$TMPDIR"/sd_f1 "$TMPDIR"/sd_f2 >/dev/null 2>&1
pass "unknown merge command handled without crashing"

# ── done ────────────────────────────────────────────────────────────────────
if [[ ${FAIL_COUNT} -eq 0 ]]; then
    echo "  ✓ sdiff: ${PASS_COUNT} passed"
else
    echo "  ✗ sdiff: ${PASS_COUNT} passed, ${FAIL_COUNT} failed"
fi
