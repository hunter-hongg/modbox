SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "-- xz --------------------------------------"

XZ_HAS_SYS=0
if command -v xz >/dev/null 2>&1; then XZ_HAS_SYS=1; fi

echo " -- help / version --"
assert_cmd_pat 'Usage:' xz --help
assert_cmd_pat 'xz \(modbox\) 1\.0' xz --version


echo " -- compress single file (T03) --"
printf 'the quick brown fox jumps over the lazy dog\n' > "$TMPDIR/c1.txt"
"$MODBOX" xz "$TMPDIR/c1.txt"
if [[ ! -f "$TMPDIR/c1.txt.xz" ]]; then fail "xz: c1.txt.xz not created"; else pass "xz: c1.txt.xz created"; fi
if [[ -f "$TMPDIR/c1.txt" ]]; then fail "xz: original c1.txt not removed"; else pass "xz: original c1.txt removed"; fi
# magic bytes
magic=$(od -An -tx1 -N2 "$TMPDIR/c1.txt.xz" | tr -d ' ')
if [[ "$magic" == "fd37" ]]; then pass "xz: magic is fd37"; else fail "xz: magic expected fd37, got $magic"; fi

echo " -- -k / compress with keep --"
printf 'repeat repeat repeat repeat repeat\n' > "$TMPDIR/keep.txt"
"$MODBOX" xz -k "$TMPDIR/keep.txt"
if [[ -f "$TMPDIR/keep.txt" && -f "$TMPDIR/keep.txt.xz" ]]; then pass "xz: -k preserves input"; else fail "xz: -k should keep input"; fi

echo " -- levels -1 vs -9 (T05) --"
mkdir -p "$TMPDIR/lvl"
rm -f "$TMPDIR/lvl"/*
python3 -c "import random; random.seed(42); print(''.join(random.choices('abcdefghijklmnopqrstuvwxyz\n', k=10000)))" > "$TMPDIR/lvl/elided.txt"
"$MODBOX" xz -1 -k "$TMPDIR/lvl/elided.txt" >/dev/null 2>&1 || true
small1=$(stat -c%s "$TMPDIR/lvl/elided.txt.xz")
rm -f "$TMPDIR/lvl/elided.txt.xz"
"$MODBOX" xz -9 -k "$TMPDIR/lvl/elided.txt" >/dev/null 2>&1 || true
small9=$(stat -c%s "$TMPDIR/lvl/elided.txt.xz")
if [[ $small1 -gt $small9 ]]; then pass "xz: level effect observed ($small1 > $small9)"; else fail "xz: -1 should be larger than -9"; fi
rm -f "$TMPDIR/lvl/elided.txt" "$TMPDIR/lvl/elided.txt.xz"
echo "" > "$TMPDIR/empty.txt"
"$MODBOX" xz "$TMPDIR/empty.txt"
if [[ -f "$TMPDIR/empty.txt.xz" ]]; then pass "xz: empty compress works"; else fail "xz: empty compress failed"; fi
rm -f "$TMPDIR/empty.txt.xz"
echo "" > "$TMPDIR/empty.txt"
"$MODBOX" xz "$TMPDIR/empty.txt"
rm -f "$TMPDIR/empty.txt.xz"
echo "" > "$TMPDIR/empty.txt"

echo " -- compress stdin -> stdout (T08) --"
"$MODBOX" xz < "$TMPDIR/empty.txt" > "$TMPDIR/stdout_pipe.bin" 2>&1
if od -An -tx1 -N2 "$TMPDIR/stdout_pipe.bin" 2>/dev/null | tr -d ' \n' | grep -q "fd37"; then pass "xz: stdin->stdout produces xz stream"; else fail "xz: stdin->stdout did not produce xz magic"; fi
rm -f "$TMPDIR/stdout_pipe.bin"

echo " -- decompress single file (T09) --"
printf 'the quick brown fox jumps over the lazy dog\n' > "$TMPDIR/d1.txt"
"$MODBOX" xz "$TMPDIR/d1.txt"
"$MODBOX" xz -d "$TMPDIR/d1.txt.xz"
if [[ ! -f "$TMPDIR/d1.txt" ]]; then fail "xz: d1.txt not restored"; else pass "xz: d1.txt restored"; fi
restored=$(cat "$TMPDIR/d1.txt")
expected='the quick brown fox jumps over the lazy dog'
if [[ "$restored" == "$expected" ]]; then pass "xz: round-trip content matches"; else fail "xz: round-trip content mismatch"; fi

echo " -- interop with system xz (best-effort) --"
printf 'interop test content for xz round-trip\n' > "$TMPDIR/interp.txt"
if [[ "$XZ_HAS_SYS" -eq 1 ]]; then
    "$MODBOX" xz "$TMPDIR/interp.txt"
    out=$(xz -d -c "$TMPDIR/interp.txt.xz")
    rm -f "$TMPDIR/interp.txt.xz"
    if [[ "$out" == "interop test content for xz round-trip" ]]; then pass "xz: interop compress/decompress ok"; else fail "xz: interop mismatch"; fi
fi

echo " -- decompress stdin (T11) --"
printf 'the quick brown fox jumps over the lazy dog\n' > "$TMPDIR/dstdin.txt"
"$MODBOX" xz "$TMPDIR/dstdin.txt"
pipeout=$(cat "$TMPDIR/dstdin.txt.xz" | "$MODBOX" xz -d)
if [[ "$pipeout" == "the quick brown fox jumps over the lazy dog" ]]; then pass "xz: stdin decompress ok"; else fail "xz: stdin decompress mismatch (got '$pipeout')"; fi
rm -f "$TMPDIR/dstdin.txt" "$TMPDIR/dstdin.txt.xz"

echo " -- -c keeps input --"
printf 'keep check content\n' > "$TMPDIR/ck.txt"
"$MODBOX" xz -k -c "$TMPDIR/ck.txt" > /tmp/ck.xz
if [[ -f "$TMPDIR/ck.txt" ]]; then pass "xz: -c -k preserves input"; else fail "xz: -c -k should preserve input"; fi

echo " -- -q suppresses .xz suffix warning --"
cp "$TMPDIR/ck.txt" "$TMPDIR/ck.txt.xz" 2>/dev/null || true
warn_out=$("$MODBOX" xz -q "$TMPDIR/ck.txt.xz" 2>&1 >/dev/null)
if [[ -n "$warn_out" ]]; then fail "xz: -q should suppress warning but got: $warn_out"; else pass "xz: -q suppresses warning"; fi

echo " -- --force overwrites output --"
printf 'force test content\n' > "$TMPDIR/force.txt"
printf 'existing xz data\n' > "$TMPDIR/force.txt.xz"
"$MODBOX" xz -f "$TMPDIR/force.txt" 2>/dev/null
if [[ -f "$TMPDIR/force.txt.xz" ]]; then pass "xz: -f overwrites existing output"; else fail "xz: -f should overwrite"; fi

echo " -- missing file error --"
out=$("$MODBOX" xz "$TMPDIR/nonexistent_file_xyz.txt" 2>&1) || true
if [[ "$out" == *"No such file"* ]] || [[ "$out" == *"cannot open"* ]] || [[ "$out" == *"nonexistent"* ]]; then pass "xz: missing file gives error"; else fail "xz: missing file should error, got: $out"; fi

echo " -- no args reads stdin --"
out=$(echo "hello from stdin" | "$MODBOX" xz 2>&1) || true
if [[ "$out" != *"Usage"* ]] && [[ "$out" != *"unknown option"* ]]; then pass "xz: no args reads stdin"; else fail "xz: no args should read stdin, got: $out"; fi

echo " -- bad arg error --"
err=$("$MODBOX" xz --nonexistent 2>&1) || true
if [[ -n "$err" ]]; then pass "xz: bad arg prints error"; else fail "xz: bad arg should print error"; fi

echo " -- round-trip through pipeline (T12) --"
pipeout=$(echo "hello" | "$MODBOX" xz | "$MODBOX" xz -d)
if [[ "$pipeout" == "hello" ]]; then pass "xz: pipeline round-trip ok"; else fail "xz: pipeline round-trip mismatch"; fi

echo " -- multiple files --"
printf 'aaa\n' > "$TMPDIR/m1.txt"
printf 'bbb\n' > "$TMPDIR/m2.txt"
"$MODBOX" xz "$TMPDIR/m1.txt" "$TMPDIR/m2.txt"
if [[ -f "$TMPDIR/m1.txt.xz" && -f "$TMPDIR/m2.txt.xz" ]]; then pass "xz: multiple files compressed"; else fail "xz: multiple files should both be compressed"; fi
"$MODBOX" xz -d "$TMPDIR/m1.txt.xz" "$TMPDIR/m2.txt.xz"
if [[ -f "$TMPDIR/m1.txt" && -f "$TMPDIR/m2.txt" ]]; then pass "xz: multiple files decompressed"; else fail "xz: multiple files should both be decompressed"; fi

echo " -- zero-length input --"
touch "$TMPDIR/zero.txt"
"$MODBOX" xz "$TMPDIR/zero.txt"
if [[ -f "$TMPDIR/zero.txt.xz" ]]; then pass "xz: zero-length compress works"; else fail "xz: zero-length compress failed"; fi
rm -f "$TMPDIR/zero.txt.xz"
echo -n "" | "$MODBOX" xz - > "$TMPDIR/zero2.xz" 2>/dev/null
if [[ -f "$TMPDIR/zero2.xz" ]]; then pass "xz: zero-length stdin compress works"; else fail "xz: zero-length stdin compress failed"; fi

echo " -- corrupt xz data --"
printf '\x00\x00\x00\x00' > "$TMPDIR/corrupt.xz"
out=$("$MODBOX" xz -d "$TMPDIR/corrupt.xz" 2>&1) || true
if [[ "$out" == *"corrupt"* ]] || [[ "$out" == *"invalid"* ]] || [[ "$out" == *"error"* ]] || [[ "$out" == *"not in"* ]]; then pass "xz: corrupt data gives error"; else fail "xz: corrupt data should error, got: $out"; fi

echo " -- suffix warning --"
printf 'suffix test\n' > "$TMPDIR/suf.txt"
"$MODBOX" xz "$TMPDIR/suf.txt"
warn_out=$("$MODBOX" xz "$TMPDIR/suf.txt.xz" 2>&1) || true
if [[ "$warn_out" == *".xz"* ]] || [[ "$warn_out" == *"already"* ]]; then pass "xz: .xz suffix warning shown"; else fail "xz: should warn about .xz suffix, got: $warn_out"; fi

echo " -- verbose output --"
printf 'verbose test\n' > "$TMPDIR/verb.txt"
"$MODBOX" xz -v "$TMPDIR/verb.txt" 2>&1 | grep -q "%" && pass "xz: verbose shows ratio" || fail "xz: verbose should show ratio"
rm -f "$TMPDIR/verb.txt.xz"

echo " -- stdout keep original --"
printf 'stdout keep\n' > "$TMPDIR/sok.txt"
"$MODBOX" xz -c "$TMPDIR/sok.txt" > /tmp/sok.xz
if [[ -f "$TMPDIR/sok.txt" ]]; then pass "xz: -c keeps original"; else fail "xz: -c should keep original"; fi

echo " -- decompress to stdout --"
printf 'dstdout\n' > "$TMPDIR/dso.txt"
"$MODBOX" xz "$TMPDIR/dso.txt"
out=$("$MODBOX" xz -dc "$TMPDIR/dso.txt.xz")
if [[ "$out" == "dstdout" ]]; then pass "xz: -dc stdout content ok"; else fail "xz: -dc content mismatch"; fi

if [[ "$XZ_HAS_SYS" -eq 1 ]]; then
    printf 'dstdout\n' > "$TMPDIR/dso2.txt"
    "$MODBOX" xz "$TMPDIR/dso2.txt"
    out=$(xz -dc "$TMPDIR/dso2.txt.xz")
    if [[ "$out" == "dstdout" ]]; then pass "xz: interop decompress matches"; else fail "xz: interop decompress mismatch"; fi
    rm -f "$TMPDIR/dso2.txt" "$TMPDIR/dso2.txt.xz"
fi

echo " -- exit 0 on success --"
printf 'exit test\n' > "$TMPDIR/exit.txt"
"$MODBOX" xz "$TMPDIR/exit.txt" >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "xz: exit 0 on success"; else fail "xz: should exit 0"; fi
"$MODBOX" xz -d "$TMPDIR/exit.txt.xz" >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "xz: decompress exit 0"; else fail "xz: should exit 0"; fi
