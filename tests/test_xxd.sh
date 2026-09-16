#!/usr/bin/env bash
# xxd — hexdump and reverse
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── xxd ────────────────────────────────────"

echo "  ── --help / --version ──"
assert_cmd_pat 'Usage:' xxd --help
assert_cmd_pat 'reverse operation' xxd --help
assert_cmd_pat 'xxd \(modbox\)' xxd --version

echo "  ── basic dump ──"
assert_cmd_pat '00000000: 4142 4344' xxd <<< 'ABCD'
assert_cmd_pat '4142 4344' xxd -c 16 <<< 'ABCD'
assert_cmd_pat '4142 4344' xxd -c 4 <<< 'ABCD'

echo "  ── grouping / offset ──"
assert_cmd_pat '4142 4344' xxd -g 2 -c 4 <<< 'ABCD'
assert_cmd_pat '0000000a:' xxd -o 10 <<< 'AB'
assert_cmd_pat '00000002:' xxd -s 2 <<< 'ABCD'

echo "  ── uppercase / plain / c-include ──"
assert_cmd_pat '6162' xxd -u <<< 'ab'
assert_cmd_pat '41424344' xxd -ps <<< 'ABCD'
assert_cmd_pat 'unsigned char stdin\[\]' xxd -i <<< 'ABCD'
assert_cmd_pat 'stdin_len' xxd -i <<< 'ABCD'

echo "  ── reverse: restore into a pipe (regression) ──"
# Historically `xxd -r` with no outfile printed "Sorry, cannot seek backwards."
# because the restore tried to fseek(2) a pipe, which is not seekable. Run the
# restore and dump the result to check the actual restored bytes.
"$MODBOX" xxd -r -ps <<< '41424344' | "$MODBOX" xxd \
    | grep -q '4142 4344' \
    && pass "xxd -r to a pipe restores the bytes" \
    || fail "xxd -r to a pipe restores the bytes"

echo "  ── reverse: restore into a regular file ──"
"$MODBOX" xxd -r -ps <<< '41424344' > "$TMPDIR/xxd_r.bin"
if cmp -s <(printf 'ABCD') "$TMPDIR/xxd_r.bin"; then
    pass "xxd -r restores bytes to a file"
else
    fail "xxd -r restores bytes to a file"
fi

echo "  ── reverse: no bogus seek error to stdout ──"
err=$("$MODBOX" xxd -r -ps <<< '41424344' 2>&1 >/dev/null)
if [[ -z "$err" ]]; then
    pass "xxd -r to stdout emits no seek error"
else
    fail "xxd -r to stdout emits no seek error (got: $err)"
fi
