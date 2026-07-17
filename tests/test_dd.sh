SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── dd ───────────────────────────────────────"

assert_files_equal() {
    local a="$1" b="$2" desc="$3"
    if cmp -s "$a" "$b"; then
        pass "$desc"
    else
        fail "$desc — files differ"
    fi
}

echo "  ── basic copy bs=1 ──"
printf 'Hello, World!\nThis is a test.\n' > "$TMPDIR"/in
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out bs=1 2>/dev/null
assert_files_equal "$TMPDIR"/in "$TMPDIR"/out "bs=1 copy"

echo "  ── basic copy bs=16 ──"
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out bs=16 2>/dev/null
assert_files_equal "$TMPDIR"/in "$TMPDIR"/out "bs=16 copy"

echo "  ── default bs (512) ──"
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out 2>/dev/null
assert_files_equal "$TMPDIR"/in "$TMPDIR"/out "default bs copy"

echo "  ── count + skip ──"
printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' > "$TMPDIR"/abc
"$MODBOX" dd if="$TMPDIR"/abc of="$TMPDIR"/out bs=2 skip=2 count=3 2>/dev/null
assert_cmd "EFGHIJ" cat "$TMPDIR"/out

echo "  ── size suffix 1K (partial block) ──"
printf '0123456789' > "$TMPDIR"/ten
"$MODBOX" dd if="$TMPDIR"/ten of="$TMPDIR"/out bs=1K count=1 2>/dev/null
assert_cmd "0123456789" cat "$TMPDIR"/out

echo "  ── x multiplier suffix ──"
"$MODBOX" dd if="$TMPDIR"/abc of="$TMPDIR"/out bs=10x2 2>/dev/null
assert_files_equal "$TMPDIR"/abc "$TMPDIR"/out "bs=10x2 copy"

echo "  ── conv=ucase ──"
printf 'Hello World\n' > "$TMPDIR"/in
printf 'HELLO WORLD\n' > "$TMPDIR"/exp
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out conv=ucase 2>/dev/null
assert_files_equal "$TMPDIR"/exp "$TMPDIR"/out "conv=ucase"

echo "  ── conv=lcase ──"
printf 'HELLO World\n' > "$TMPDIR"/in
printf 'hello world\n' > "$TMPDIR"/exp
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out conv=lcase 2>/dev/null
assert_files_equal "$TMPDIR"/exp "$TMPDIR"/out "conv=lcase"

echo "  ── conv=swab is its own inverse ──"
printf 'ABCDEFGH' > "$TMPDIR"/in
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/mid conv=swab 2>/dev/null
"$MODBOX" dd if="$TMPDIR"/mid of="$TMPDIR"/out conv=swab 2>/dev/null
assert_files_equal "$TMPDIR"/in "$TMPDIR"/out "conv=swab twice == original"

echo "  ── conv=sync pads to ibs ──"
printf 'abc' > "$TMPDIR"/in
printf 'abc\0\0' > "$TMPDIR"/exp
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out bs=5 conv=sync 2>/dev/null
assert_files_equal "$TMPDIR"/exp "$TMPDIR"/out "conv=sync pads to ibs"

echo "  ── ascii/ebcdic roundtrip ──"
printf 'The Quick Brown Fox 0123\n' > "$TMPDIR"/in
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/mid conv=ebcdic 2>/dev/null
"$MODBOX" dd if="$TMPDIR"/mid of="$TMPDIR"/out conv=ascii 2>/dev/null
assert_files_equal "$TMPDIR"/in "$TMPDIR"/out "ebcdic->ascii roundtrip"

echo "  ── conv=block / unblock roundtrip ──"
printf 'one\ntwo\nthree\n' > "$TMPDIR"/in
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/mid conv=block cbs=8 2>/dev/null
"$MODBOX" dd if="$TMPDIR"/mid of="$TMPDIR"/out conv=unblock cbs=8 2>/dev/null
assert_files_equal "$TMPDIR"/in "$TMPDIR"/out "block->unblock roundtrip"

echo "  ── count_bytes / skip_bytes (B suffix) ──"
printf '0123456789' > "$TMPDIR"/ten
"$MODBOX" dd if="$TMPDIR"/ten of="$TMPDIR"/out bs=16 skip=3B count=4B 2>/dev/null
assert_cmd "3456" cat "$TMPDIR"/out

echo "  ── seek + notrunc ──"
printf 'AAAA\n' > "$TMPDIR"/base
printf 'BBBB\n' > "$TMPDIR"/in
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/base bs=1 seek=2 conv=notrunc 2>/dev/null
assert_cmd "$(printf 'AABBBB\n')" cat "$TMPDIR"/base

echo "  ── status=noxfer suppresses bytes stat ──"
"$MODBOX" dd if="$TMPDIR"/in of=/dev/null bs=4 status=noxfer 2>&1 1>/dev/null | grep -q "bytes copied"
if [[ $? -ne 0 ]]; then pass "status=noxfer omits bytes"; else fail "status=noxfer omits bytes"; fi

echo "  ── status=none emits nothing ──"
out=$("$MODBOX" dd if="$TMPDIR"/in of=/dev/null bs=4 status=none 2>&1 1>/dev/null || true)
if [[ -z "$out" ]]; then pass "status=none silent"; else fail "status=none silent — got [$out]"; fi

echo "  ── invalid conversion ──"
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out conv=bogus 2>&1 1>/dev/null | grep -q "invalid conversion"
if [[ $? -eq 0 ]]; then pass "invalid conv rejected"; else fail "invalid conv rejected"; fi

echo "  ── ascii+ebcdic rejected ──"
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/out conv=ascii,ebcdic 2>&1 1>/dev/null | grep -q "cannot combine"
if [[ $? -eq 0 ]]; then pass "ascii+ebcdic rejected"; else fail "ascii+ebcdic rejected"; fi

echo "  ── excl on existing file ──"
printf 'x' > "$TMPDIR"/ex
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/ex bs=1 conv=excl 2>&1 1>/dev/null | grep -q "File exists"
if [[ $? -eq 0 ]]; then pass "conv=excl on existing file"; else fail "conv=excl on existing file"; fi

echo "  ── nocreat on missing file ──"
"$MODBOX" dd if="$TMPDIR"/in of="$TMPDIR"/nope bs=1 conv=nocreat 2>&1 1>/dev/null | grep -q "No such file"
if [[ $? -eq 0 ]]; then pass "conv=nocreat on missing file"; else fail "conv=nocreat on missing file"; fi

echo "  ── help ──"
assert_cmd_pat 'Usage:' dd --help

echo "  ── no operands (reads stdin, writes stdout) ──"
result=$(printf 'data\n' | "$MODBOX" dd bs=4096 2>/dev/null || true)
if [[ "$result" == "data" ]]; then pass "dd stdin->stdout"; else fail "dd stdin->stdout — got [$result]"; fi
