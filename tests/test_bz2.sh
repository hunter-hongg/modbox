SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── bzip2 ─────────────────────────────────────"

BZIP2_HAS_SYS=0
if command -v bzip2 >/dev/null 2>&1 && command -v bunzip2 >/dev/null 2>&1; then BZIP2_HAS_SYS=1; fi

echo "  ── help / version ──"
assert_cmd_pat 'Usage:.*bzip2.*\[FILE\]' bzip2 --help
assert_cmd_pat 'bzip2 \(modbox\) 1\.0' bzip2 --version
assert_cmd_pat 'decompress' bunzip2 --help
assert_cmd_pat 'standard output' bzcat --help

echo "  ── compress single file ──"
printf 'the quick brown fox jumps over the lazy dog\n' > "$TMPDIR/c1.txt"
"$MODBOX" bzip2 "$TMPDIR/c1.txt"
if [[ -f "$TMPDIR/c1.txt.bz2" ]]; then pass "bzip2: c1.txt.bz2 created"; else fail "bzip2: c1.txt.bz2 not created"; fi
if [[ -f "$TMPDIR/c1.txt" ]]; then fail "bzip2: original c1.txt not removed"; else pass "bzip2: original c1.txt removed"; fi
magic=$(od -An -tx1 -N3 "$TMPDIR/c1.txt.bz2" | tr -d ' ')
if [[ "$magic" == "425a68" ]]; then pass "bzip2: output begins with 'BZh'"; else fail "bzip2: magic is [$magic] expected 425a68"; fi

echo "  ── keep / stdout / force ──"
printf 'keep me\n' > "$TMPDIR/k.txt"
"$MODBOX" bzip2 -k "$TMPDIR/k.txt"
if [[ -f "$TMPDIR/k.txt" && -f "$TMPDIR/k.txt.bz2" ]]; then pass "bzip2 -k: keeps original and creates .bz2"; else fail "bzip2 -k: state wrong"; fi

printf 'stdout me\n' > "$TMPDIR/s.txt"
out=$("$MODBOX" bzip2 -c "$TMPDIR/s.txt" 2>/dev/null | od -An -tx1 -N3 | tr -d ' ')
if [[ "$out" == "425a68" && -f "$TMPDIR/s.txt" ]]; then pass "bzip2 -c: writes magic to stdout, keeps file"; else fail "bzip2 -c: out=[$out]"; fi

printf 'force me\n' > "$TMPDIR/f.txt"
"$MODBOX" bzip2 "$TMPDIR/f.txt" >/dev/null 2>&1
printf 'force me again\n' > "$TMPDIR/f.txt"
"$MODBOX" bzip2 -f "$TMPDIR/f.txt" >/dev/null 2>&1
if [[ -f "$TMPDIR/f.txt.bz2" ]]; then pass "bzip2 -f: overwrites existing .bz2"; else fail "bzip2 -f: did not overwrite"; fi

printf 'no force\n' > "$TMPDIR/nf.txt"
"$MODBOX" bzip2 "$TMPDIR/nf.txt" >/dev/null 2>&1
printf 'no force again\n' > "$TMPDIR/nf.txt"
if "$MODBOX" bzip2 "$TMPDIR/nf.txt" >/dev/null 2>&1; then fail "bzip2: existing .bz2 without -f should fail"; else pass "bzip2: existing .bz2 without -f fails"; fi

echo "  ── decompress + round-trip ──"
printf 'round trip payload\n' > "$TMPDIR/rt.txt"
cp "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"
"$MODBOX" bzip2 "$TMPDIR/rt.txt" >/dev/null 2>&1
"$MODBOX" bzip2 -d "$TMPDIR/rt.txt.bz2" >/dev/null 2>&1
if [[ ! -f "$TMPDIR/rt.txt.bz2" && -f "$TMPDIR/rt.txt" ]]; then pass "bzip2 -d: restores file, removes .bz2"; else fail "bzip2 -d: restore state wrong"; fi
if cmp -s "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"; then pass "bzip2: round-trip byte-identical"; else fail "bzip2: round-trip mismatch"; fi

printf 'dc stream\n' > "$TMPDIR/dc.txt"
"$MODBOX" bzip2 "$TMPDIR/dc.txt" >/dev/null 2>&1
dc_out=$("$MODBOX" bzip2 -dc "$TMPDIR/dc.txt.bz2" 2>/dev/null)
if [[ "$dc_out" == "dc stream" ]]; then pass "bzip2 -dc: writes decompressed bytes to stdout"; else fail "bzip2 -dc: got [$dc_out]"; fi

printf 'keep decompressed\n' > "$TMPDIR/dk.txt"
"$MODBOX" bzip2 "$TMPDIR/dk.txt" >/dev/null 2>&1
"$MODBOX" bzip2 -dk "$TMPDIR/dk.txt.bz2" >/dev/null 2>&1
if [[ -f "$TMPDIR/dk.txt" && -f "$TMPDIR/dk.txt.bz2" ]]; then pass "bzip2 -dk: restores file and keeps .bz2"; else fail "bzip2 -dk: state wrong"; fi

echo "  ── bunzip2 / bzcat names dispatch on argv[0] ──"
ln -sf "$MODBOX" "$TMPDIR/bunzip2"
ln -sf "$MODBOX" "$TMPDIR/bzcat"
printf 'dispatched by name\n' > "$TMPDIR/name.txt"
"$MODBOX" bzip2 "$TMPDIR/name.txt" >/dev/null 2>&1
"$TMPDIR/bunzip2" "$TMPDIR/name.txt.bz2" >/dev/null 2>&1
if [[ -f "$TMPDIR/name.txt" && ! -f "$TMPDIR/name.txt.bz2" ]]; then pass "bunzip2: decompresses by default (argv[0])"; else fail "bunzip2: argv[0] dispatch wrong"; fi

printf 'cat by name\n' > "$TMPDIR/catn.txt"
"$MODBOX" bzip2 "$TMPDIR/catn.txt" >/dev/null 2>&1
catname_out=$("$TMPDIR/bzcat" "$TMPDIR/catn.txt.bz2" 2>/dev/null)
if [[ "$catname_out" == "cat by name" && -f "$TMPDIR/catn.txt.bz2" ]]; then pass "bzcat: decompresses to stdout and keeps input"; else fail "bzcat: got [$catname_out]"; fi

echo "  ── bzcat -f passes non-bzip2 input through ──"
# Upstream's `bzcat -f FILE` decompresses bzip2 and copies anything else
# through, which is how bzcat doubles as a plain `cat`. Without -f a
# non-bzip2 file is an error, so the flag is what selects the two behaviours.
printf 'not compressed at all\n' > "$TMPDIR/plain.txt"
force_out=$("$MODBOX" bzcat -f "$TMPDIR/plain.txt" 2>/dev/null)
if [[ "$force_out" == "not compressed at all" ]]; then
    pass "bzcat -f: copies non-bzip2 input to stdout"
else
    fail "bzcat -f: got [$force_out]"
fi
noforce_out=$("$MODBOX" bzcat "$TMPDIR/plain.txt" 2>/dev/null)
if [[ -z "$noforce_out" ]]; then
    pass "bzcat (no -f): refuses non-bzip2 input"
else
    fail "bzcat (no -f): unexpectedly produced [$noforce_out]"
fi
assert_cmd_pat_stderr 'not a bzip2 file' bzcat "$TMPDIR/plain.txt"
# -f must not swallow a real corruption: bad magic is passthrough-eligible,
# a broken stream is not.
printf 'BZh9truncated garbage' > "$TMPDIR/broken.bz2"
"$MODBOX" bzcat -f "$TMPDIR/broken.bz2" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then
    pass "bzcat -f: still fails on a corrupt bzip2 stream"
else
    fail "bzcat -f: corrupt stream was passed through"
fi
# The passthrough is bzcat-specific; bunzip2 -f must keep erroring.
"$MODBOX" bunzip2 -f "$TMPDIR/plain.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then
    pass "bunzip2 -f: does not inherit the bzcat passthrough"
else
    fail "bunzip2 -f: unexpectedly accepted a non-bzip2 file"
fi
# A mixed directory is the reason the feature exists.
printf 'plain member\n' > "$TMPDIR/mix-plain.txt"
printf 'bz member\n'     > "$TMPDIR/mix.txt"
"$MODBOX" bzip2 "$TMPDIR/mix.txt" >/dev/null 2>&1
mix_out=$("$MODBOX" bzcat -f "$TMPDIR/mix-plain.txt" "$TMPDIR/mix.txt.bz2" 2>/dev/null)
if [[ "$mix_out" == "plain member
bz member" ]]; then
    pass "bzcat -f: handles a mixed plain+compressed directory"
else
    fail "bzcat -f mixed dir: got [$mix_out]"
fi

echo "  ── -d on non-.bz2 name appends .out ──"
printf 'odd name payload\n' > "$TMPDIR/odd.src"
"$MODBOX" bzip2 -k "$TMPDIR/odd.src" >/dev/null 2>&1
cp "$TMPDIR/odd.src.bz2" "$TMPDIR/weird.dat"
"$MODBOX" bzip2 -d "$TMPDIR/weird.dat" >/dev/null 2>&1
if [[ -f "$TMPDIR/weird.dat.out" ]] && cmp -s "$TMPDIR/weird.dat.out" "$TMPDIR/odd.src"; then pass "bzip2 -d: non-.bz2 input writes <name>.out"; else fail "bzip2 -d: <name>.out not produced"; fi

echo "  ── multi-file ──"
printf 'aaa\n' > "$TMPDIR/m1.txt"
printf 'bbb\n' > "$TMPDIR/m2.txt"
printf 'ccc\n' > "$TMPDIR/m3.txt"
"$MODBOX" bzip2 "$TMPDIR/m1.txt" "$TMPDIR/m2.txt" "$TMPDIR/m3.txt" >/dev/null 2>&1
if [[ -f "$TMPDIR/m1.txt.bz2" && -f "$TMPDIR/m2.txt.bz2" && -f "$TMPDIR/m3.txt.bz2" ]]; then pass "bzip2: each file gets its own .bz2"; else fail "bzip2: multi-file .bz2 missing"; fi
"$MODBOX" bzip2 -d "$TMPDIR/m1.txt.bz2" "$TMPDIR/m2.txt.bz2" "$TMPDIR/m3.txt.bz2" >/dev/null 2>&1
if [[ -f "$TMPDIR/m1.txt" && -f "$TMPDIR/m2.txt" && -f "$TMPDIR/m3.txt" ]]; then pass "bzip2 -d: multi-file decompresses each"; else fail "bzip2 -d: multi-file restore missing"; fi

echo "  ── stdin/stdout pipelines ──"
pipe_out=$(printf 'pipe data\n' | "$MODBOX" bzip2 2>/dev/null | od -An -tx1 -N3 | tr -d ' ')
if [[ "$pipe_out" == "425a68" ]]; then pass "bzip2: no-arg compresses stdin to stdout"; else fail "bzip2: stdin->stdout magic [$pipe_out]"; fi
round=$(printf 'pipe data\n' | "$MODBOX" bzip2 2>/dev/null | "$MODBOX" bzip2 -d 2>/dev/null)
if [[ "$round" == "pipe data" ]]; then pass "bzip2 -d: no-arg decompresses stdin to stdout"; else fail "bzip2 -d: stdin->stdout got [$round]"; fi

echo "  ── -t integrity test ──"
printf 'integrity payload\n' > "$TMPDIR/t.txt"
"$MODBOX" bzip2 -k "$TMPDIR/t.txt" >/dev/null 2>&1
if "$MODBOX" bzip2 -t "$TMPDIR/t.txt.bz2" >/dev/null 2>&1; then pass "bzip2 -t: valid file exits 0"; else fail "bzip2 -t: valid file should exit 0"; fi
cp "$TMPDIR/t.txt.bz2" "$TMPDIR/t.bad.bz2"
printf 'X' | dd of="$TMPDIR/t.bad.bz2" bs=1 seek=20 conv=notrunc >/dev/null 2>&1
if "$MODBOX" bzip2 -t "$TMPDIR/t.bad.bz2" >/dev/null 2>&1; then fail "bzip2 -t: corrupt file should fail"; else pass "bzip2 -t: corrupt file exits non-zero"; fi

echo "  ── block-size levels accepted ──"
printf 'level content\n' > "$TMPDIR/lv.txt"
"$MODBOX" bzip2 -1 -k "$TMPDIR/lv.txt" >/dev/null 2>&1
printf 'level content\n' > "$TMPDIR/lv9.txt"
"$MODBOX" bzip2 -9 -k "$TMPDIR/lv9.txt" >/dev/null 2>&1
printf 'best content\n' > "$TMPDIR/be.txt"
"$MODBOX" bzip2 --best -k "$TMPDIR/be.txt" >/dev/null 2>&1
printf 'fast content\n' > "$TMPDIR/fa.txt"
"$MODBOX" bzip2 --fast -k "$TMPDIR/fa.txt" >/dev/null 2>&1
if [[ -s "$TMPDIR/lv.txt.bz2" && -s "$TMPDIR/lv9.txt.bz2" && -s "$TMPDIR/be.txt.bz2" && -s "$TMPDIR/fa.txt.bz2" ]]; then pass "bzip2: -1/-9/--fast/--best all produce output"; else fail "bzip2: a level flag produced nothing"; fi
# block size is recorded in the header (4th byte is '1'..'9')
lvl9=$(od -An -c -N4 "$TMPDIR/lv9.txt.bz2" | tr -s ' ' | cut -d' ' -f5)
if [[ "$lvl9" == "9" ]]; then pass "bzip2 -9: header records block size 9"; else fail "bzip2 -9: header block-size byte [$lvl9]"; fi

echo "  ── error handling ──"
assert_cmd_pat_stderr 'No such file' bzip2 "$TMPDIR/does_not_exist_xyz.txt"
if "$MODBOX" bzip2 -d "$TMPDIR/does_not_exist_xyz.txt" >/dev/null 2>&1; then fail "bzip2 -d: missing file should be non-zero"; else pass "bzip2 -d: missing file exits non-zero"; fi

printf 'hello not bzip2\n' > "$TMPDIR/plain.txt"
if "$MODBOX" bzip2 -d "$TMPDIR/plain.txt" >/dev/null 2>&1; then fail "bzip2 -d: non-bzip2 should fail"; else pass "bzip2 -d: non-bzip2 file exits non-zero"; fi
assert_cmd_pat_stderr 'not a bzip2 file' bzip2 -d "$TMPDIR/plain.txt"

assert_cmd_pat_stderr 'unrecognized option' bzip2 --nope

printf 'suffix guard\n' > "$TMPDIR/g.txt"
"$MODBOX" bzip2 "$TMPDIR/g.txt" >/dev/null 2>&1
assert_cmd_pat_stderr 'already has .bz2 suffix' bzip2 "$TMPDIR/g.txt.bz2"

echo "  ── zero-length input ──"
: > "$TMPDIR/empty.txt"
"$MODBOX" bzip2 "$TMPDIR/empty.txt" >/dev/null 2>&1
emptymagic=$(od -An -tx1 -N3 "$TMPDIR/empty.txt.bz2" 2>/dev/null | tr -d ' ')
if [[ "$emptymagic" == "425a68" ]]; then pass "bzip2: zero-length input -> valid small .bz2"; else fail "bzip2: empty .bz2 magic [$emptymagic]"; fi
"$MODBOX" bzip2 -d "$TMPDIR/empty.txt.bz2" >/dev/null 2>&1
if [[ -f "$TMPDIR/empty.txt" && ! -s "$TMPDIR/empty.txt" ]]; then pass "bzip2 -d: empty .bz2 restores empty file"; else fail "bzip2 -d: empty restore wrong"; fi

echo "  ── exit code with mixed good/bad ──"
printf 'good file\n' > "$TMPDIR/good.txt"
"$MODBOX" bzip2 "$TMPDIR/does_not_exist_xyz.txt" "$TMPDIR/good.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "bzip2: mix of bad+good exits non-zero"; else fail "bzip2: mix should exit non-zero"; fi

echo "  ── interop with system bzip2 (best-effort) ──"
if [[ $BZIP2_HAS_SYS -eq 1 ]]; then
    printf 'interop payload 12345\n' > "$TMPDIR/io.txt"
    "$MODBOX" bzip2 -k "$TMPDIR/io.txt" >/dev/null 2>&1
    sys_out=$(bunzip2 -c "$TMPDIR/io.txt.bz2" 2>/dev/null)
    if [[ "$sys_out" == "interop payload 12345" ]]; then pass "bzip2: system bunzip2 reads modbox .bz2"; else fail "bzip2: system bunzip2 mismatch [$sys_out]"; fi
    printf 'reverse interop\n' > "$TMPDIR/rio.txt"
    bzip2 -c "$TMPDIR/rio.txt" > "$TMPDIR/rio.txt.bz2" 2>/dev/null
    mb_out=$("$MODBOX" bzip2 -dc "$TMPDIR/rio.txt.bz2" 2>/dev/null)
    if [[ "$mb_out" == "reverse interop" ]]; then pass "bzip2: modbox reads system .bz2"; else fail "bzip2: modbox read of system .bz2 mismatch [$mb_out]"; fi
    # concatenated streams
    { bzip2 -c "$TMPDIR/rio.txt"; bzip2 -c "$TMPDIR/rio.txt"; } > "$TMPDIR/cc.bz2" 2>/dev/null
    cc_out=$("$MODBOX" bzip2 -dc "$TMPDIR/cc.bz2" 2>/dev/null)
    if [[ "$cc_out" == "reverse interop
reverse interop" ]]; then pass "bzip2: concatenated streams decode together"; else fail "bzip2: concatenated decode got [$cc_out]"; fi
else
    echo "  SKIP interop (system bzip2 absent)"
fi
