SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── gzip ──────────────────────────────────────"

GZIP_HAS_SYS=0
if command -v gzip >/dev/null 2>&1; then GZIP_HAS_SYS=1; fi

echo "  ── help / version ──"
assert_cmd_pat 'Usage:' gzip --help
assert_cmd_pat 'gzip \(modbox\) 1\.0' gzip --version


echo "  ── compress single file (T02) ──"
printf 'the quick brown fox jumps over the lazy dog\n' > "$TMPDIR/c1.txt"
"$MODBOX" gzip "$TMPDIR/c1.txt"
if [[ ! -f "$TMPDIR/c1.txt.gz" ]]; then fail "gzip: c1.txt.gz not created"; else pass "gzip: c1.txt.gz created"; fi
if [[ -f "$TMPDIR/c1.txt" ]]; then fail "gzip: original c1.txt not removed"; else pass "gzip: original c1.txt removed"; fi
# magic bytes
magic=$(od -An -tx1 -N2 "$TMPDIR/c1.txt.gz" | tr -d ' ')
if [[ "$magic" == "1f8b" ]]; then pass "gzip: output begins with 1f 8b"; else fail "gzip: magic is [$magic] expected 1f8b"; fi

echo "  ── levels -1 vs -9 (T02) ──"
printf 'repeat repeat repeat repeat repeat repeat repeat repeat\n' > "$TMPDIR/lvl.txt"
"$MODBOX" gzip -1 "$TMPDIR/lvl.txt"; mv "$TMPDIR/lvl.txt.gz" "$TMPDIR/lvl1.gz"
printf 'repeat repeat repeat repeat repeat repeat repeat repeat\n' > "$TMPDIR/lvl.txt"
"$MODBOX" gzip -9 "$TMPDIR/lvl.txt"; mv "$TMPDIR/lvl.txt.gz" "$TMPDIR/lvl9.gz"
if [[ -s "$TMPDIR/lvl1.gz" && -s "$TMPDIR/lvl9.gz" ]]; then pass "gzip: -1 and -9 both produce a .gz"; else fail "gzip: -1/-9 produced empty"; fi
# default level 6
printf 'default level content here\n' > "$TMPDIR/dfl.txt"
"$MODBOX" gzip "$TMPDIR/dfl.txt"
if [[ -s "$TMPDIR/dfl.txt.gz" ]]; then pass "gzip: default level 6 produces valid .gz"; else fail "gzip: default level produced empty"; fi
# --fast / --best
printf 'fast best content\n' > "$TMPDIR/fb.txt"
"$MODBOX" gzip --fast "$TMPDIR/fb.txt"; mv "$TMPDIR/fb.txt.gz" "$TMPDIR/fb_fast.gz"
printf 'fast best content\n' > "$TMPDIR/fb.txt"
"$MODBOX" gzip --best "$TMPDIR/fb.txt"; mv "$TMPDIR/fb.txt.gz" "$TMPDIR/fb_best.gz"
if [[ -s "$TMPDIR/fb_fast.gz" && -s "$TMPDIR/fb_best.gz" ]]; then pass "gzip: --fast and --best produce .gz"; else fail "gzip: --fast/--best failed"; fi

echo "  ── keep / stdout / force (T03) ──"
printf 'keep me\n' > "$TMPDIR/k.txt"
"$MODBOX" gzip -k "$TMPDIR/k.txt"
if [[ -f "$TMPDIR/k.txt" && -f "$TMPDIR/k.txt.gz" ]]; then pass "gzip -k: keeps original and creates .gz"; else fail "gzip -k: original/compressed state wrong"; fi

printf 'stdout me\n' > "$TMPDIR/s.txt"
out=$("$MODBOX" gzip -c "$TMPDIR/s.txt" 2>/dev/null | od -An -tx1 -N2 | tr -d ' ')
if [[ "$out" == "1f8b" && -f "$TMPDIR/s.txt" ]]; then pass "gzip -c: writes magic to stdout, keeps file"; else fail "gzip -c: out=[$out] file=[$([[ -f "$TMPDIR/s.txt" ]] && echo yes || echo no)]"; fi

printf 'force me\n' > "$TMPDIR/f.txt"
"$MODBOX" gzip "$TMPDIR/f.txt" >/dev/null 2>&1
printf 'force me again\n' > "$TMPDIR/f.txt"
"$MODBOX" gzip -f "$TMPDIR/f.txt" >/dev/null 2>&1
if [[ -f "$TMPDIR/f.txt.gz" ]]; then pass "gzip -f: overwrites existing .gz"; else fail "gzip -f: did not overwrite"; fi
# without -f: error
printf 'no force\n' > "$TMPDIR/nf.txt"
"$MODBOX" gzip "$TMPDIR/nf.txt" >/dev/null 2>&1
printf 'no force again\n' > "$TMPDIR/nf.txt"
if "$MODBOX" gzip "$TMPDIR/nf.txt" >/dev/null 2>&1; then fail "gzip: existing .gz without -f should fail"; else pass "gzip: existing .gz without -f fails"; fi

echo "  ── decompress + round-trip (T04) ──"
printf 'round trip payload\n' > "$TMPDIR/rt.txt"
cp "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"
"$MODBOX" gzip "$TMPDIR/rt.txt" >/dev/null 2>&1
"$MODBOX" gzip -d "$TMPDIR/rt.txt.gz" >/dev/null 2>&1
if [[ ! -f "$TMPDIR/rt.txt.gz" && -f "$TMPDIR/rt.txt" ]]; then pass "gzip -d: restores file, removes .gz"; else fail "gzip -d: restore state wrong"; fi
if cmp -s "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"; then pass "gzip: round-trip byte-identical"; else fail "gzip: round-trip mismatch"; fi
# -dc to stdout
printf 'dc stream\n' > "$TMPDIR/dc.txt"
"$MODBOX" gzip "$TMPDIR/dc.txt" >/dev/null 2>&1
dc_out=$("$MODBOX" gzip -dc "$TMPDIR/dc.txt.gz" 2>/dev/null)
if [[ "$dc_out" == "dc stream" ]]; then pass "gzip -dc: writes decompressed bytes to stdout"; else fail "gzip -dc: got [$dc_out]"; fi

# -dk keeps the .gz
printf 'keep decompressed\n' > "$TMPDIR/dk.txt"
"$MODBOX" gzip "$TMPDIR/dk.txt" >/dev/null 2>&1
"$MODBOX" gzip -dk "$TMPDIR/dk.txt.gz" >/dev/null 2>&1
if [[ -f "$TMPDIR/dk.txt" && -f "$TMPDIR/dk.txt.gz" ]]; then pass "gzip -dk: restores file and keeps .gz"; else fail "gzip -dk: state wrong"; fi


echo "  ── multi-file (T05) ──"
printf 'aaa\n' > "$TMPDIR/m1.txt"
printf 'bbb\n' > "$TMPDIR/m2.txt"
printf 'ccc\n' > "$TMPDIR/m3.txt"
"$MODBOX" gzip "$TMPDIR/m1.txt" "$TMPDIR/m2.txt" "$TMPDIR/m3.txt" >/dev/null 2>&1
if [[ -f "$TMPDIR/m1.txt.gz" && -f "$TMPDIR/m2.txt.gz" && -f "$TMPDIR/m3.txt.gz" ]]; then pass "gzip: each file gets its own .gz"; else fail "gzip: multi-file .gz missing"; fi
"$MODBOX" gzip -d "$TMPDIR/m1.txt.gz" "$TMPDIR/m2.txt.gz" "$TMPDIR/m3.txt.gz" >/dev/null 2>&1
if [[ -f "$TMPDIR/m1.txt" && -f "$TMPDIR/m2.txt" && -f "$TMPDIR/m3.txt" ]]; then pass "gzip -d: multi-file decompresses each"; else fail "gzip -d: multi-file restore missing"; fi

echo "  ── stdin/stdout pipelines (T05) ──"
pipe_out=$(printf 'pipe data\n' | "$MODBOX" gzip 2>/dev/null | od -An -tx1 -N2 | tr -d ' ')
if [[ "$pipe_out" == "1f8b" ]]; then pass "gzip: no-arg compresses stdin to stdout"; else fail "gzip: stdin->stdout magic [$pipe_out]"; fi
round=$(printf 'pipe data\n' | "$MODBOX" gzip 2>/dev/null | "$MODBOX" gzip -d 2>/dev/null)
if [[ "$round" == "pipe data" ]]; then pass "gzip -d: no-arg decompresses stdin to stdout"; else fail "gzip -d: stdin->stdout got [$round]"; fi

echo "  ── error handling (T06) ──"
"$MODBOX" gzip "$TMPDIR/does_not_exist_xyz.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "gzip: missing file exits non-zero"; else fail "gzip: missing file should exit non-zero"; fi
assert_cmd_pat_stderr 'No such file' gzip "$TMPDIR/does_not_exist_xyz.txt"

# corrupt/truncated .gz (valid magic, bad payload)
printf '\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x00\x00garbage' > "$TMPDIR/bad.gz"
"$MODBOX" gzip -d "$TMPDIR/bad.gz" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "gzip -d: corrupt/truncated .gz exits non-zero"; else fail "gzip -d: corrupt .gz should fail"; fi
assert_cmd_pat_stderr 'unexpected end of file|invalid compressed data' gzip -d "$TMPDIR/bad.gz"

# non-gzip file
printf 'hello not gzip\n' > "$TMPDIR/plain.txt"
"$MODBOX" gzip -d "$TMPDIR/plain.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "gzip -d: non-gzip file exits non-zero"; else fail "gzip -d: non-gzip should fail"; fi
assert_cmd_pat_stderr 'not in gzip format' gzip -d "$TMPDIR/plain.txt"

echo "  ── zero-length input (T06) ──"
: > "$TMPDIR/empty.txt"
"$MODBOX" gzip "$TMPDIR/empty.txt" >/dev/null 2>&1
emptymagic=$(od -An -tx1 -N2 "$TMPDIR/empty.txt.gz" 2>/dev/null | tr -d ' ')
if [[ "$emptymagic" == "1f8b" ]]; then pass "gzip: zero-length input -> valid small .gz"; else fail "gzip: empty .gz magic [$emptymagic]"; fi
"$MODBOX" gzip -d "$TMPDIR/empty.txt.gz" >/dev/null 2>&1
if [[ -f "$TMPDIR/empty.txt" && ! -s "$TMPDIR/empty.txt" ]]; then pass "gzip -d: empty .gz restores empty file"; else fail "gzip -d: empty restore wrong"; fi

echo "  ── exit code only when all succeed (T06) ──"
printf 'good file\n' > "$TMPDIR/good.txt"
"$MODBOX" gzip "$TMPDIR/does_not_exist_xyz.txt" "$TMPDIR/good.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "gzip: mix of bad+good exits non-zero"; else fail "gzip: mix should exit non-zero"; fi

echo "  ── -q suppresses warnings (T06) ──"
printf 'warn content\n' > "$TMPDIR/w.txt"
"$MODBOX" gzip "$TMPDIR/w.txt" >/dev/null 2>&1   # creates w.txt.gz, removes w.txt
"$MODBOX" gzip -q "$TMPDIR/w.txt.gz" >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "gzip -q: suppresses .gz-suffix warning (exit 0)"; else fail "gzip -q: should exit 0"; fi
warn_out=$("$MODBOX" gzip "$TMPDIR/w.txt.gz" 2>&1 >/dev/null)

if [[ -n "$warn_out" ]]; then pass "gzip: without -q warns about .gz suffix"; else fail "gzip: expected .gz-suffix warning"; fi

echo "  ── verbosity (T02/T04) ──"
printf 'verbose content line\n' > "$TMPDIR/v.txt"
assert_cmd_pat 'replaced with' gzip -v "$TMPDIR/v.txt"

echo ""
echo "── gunzip ──────────────────────────────────────"

echo "  ── help / version ──"
assert_cmd_pat 'Usage:.*gunzip.*\[FILE\]' gunzip --help
assert_cmd_pat 'gunzip \(modbox\) 1\.0' gunzip --version

echo "  ── default action: decompress (no -d needed) ──"
printf 'gunzip default payload\n' > "$TMPDIR/gd.txt"
cp "$TMPDIR/gd.txt" "$TMPDIR/gd.orig"
"$MODBOX" gzip "$TMPDIR/gd.txt" >/dev/null 2>&1
"$MODBOX" gunzip "$TMPDIR/gd.txt.gz" >/dev/null 2>&1
if [[ ! -f "$TMPDIR/gd.txt.gz" && -f "$TMPDIR/gd.txt" ]]; then
    pass "gunzip: decompresses by default"
else
    fail "gunzip: default decompress state wrong"
fi
if cmp -s "$TMPDIR/gd.txt" "$TMPDIR/gd.orig"; then
    pass "gunzip: round-trip byte-identical"
else
    fail "gunzip: round-trip mismatch"
fi

echo "  ── gzip compress, gunzip decompress cross-name ──"
printf 'cross name payload\n' > "$TMPDIR/cn.txt"
cp "$TMPDIR/cn.txt" "$TMPDIR/cn.orig"
"$MODBOX" gzip "$TMPDIR/cn.txt" >/dev/null 2>&1
"$MODBOX" gunzip "$TMPDIR/cn.txt.gz" >/dev/null 2>&1
if cmp -s "$TMPDIR/cn.txt" "$TMPDIR/cn.orig"; then
    pass "gunzip: gzip->gunzip round-trip matches original"
else
    fail "gunzip: gzip->gunzip round-trip mismatch"
fi

echo "  ── stdin/stdout pipeline ──"
printf 'gunzip pipe payload\n' > "$TMPDIR/gp.txt"
"$MODBOX" gzip "$TMPDIR/gp.txt" >/dev/null 2>&1
pipe_out=$("$MODBOX" gunzip < "$TMPDIR/gp.txt.gz" 2>/dev/null)
if [[ "$pipe_out" == "gunzip pipe payload" ]]; then
    pass "gunzip: stdin -> stdout"
else
    fail "gunzip: stdin->stdout got [$pipe_out]"
fi

echo "  ── -k keeps compressed file ──"
printf 'gunzip keep\n' > "$TMPDIR/gk.txt"
"$MODBOX" gzip "$TMPDIR/gk.txt" >/dev/null 2>&1
"$MODBOX" gunzip -k "$TMPDIR/gk.txt.gz" >/dev/null 2>&1
if [[ -f "$TMPDIR/gk.txt" && -f "$TMPDIR/gk.txt.gz" ]]; then
    pass "gunzip -k: keeps compressed file"
else
    fail "gunzip -k: state wrong"
fi

echo "  ── -c writes decompressed bytes to stdout ──"
printf 'gunzip -c\n' > "$TMPDIR/gc.txt"
"$MODBOX" gzip "$TMPDIR/gc.txt" >/dev/null 2>&1
c_out=$("$MODBOX" gunzip -c "$TMPDIR/gc.txt.gz" 2>/dev/null)
if [[ "$c_out" == "gunzip -c" ]]; then
    pass "gunzip -c: writes decompressed bytes to stdout"
else
    fail "gunzip -c: got [$c_out]"
fi
# -c must KEEP the input. The compress direction already did; the decompress
# direction used to delete it, which the stdout-content assertion above could
# not see because the bytes are identical either way.
if [[ -f "$TMPDIR/gc.txt.gz" ]]; then
    pass "gunzip -c: keeps the compressed input"
else
    fail "gunzip -c: deleted the compressed input"
fi
# Same contract via the combined form, and it must not write a decompressed
# file next to the input.
printf 'gzip -dc\n' > "$TMPDIR/gdc.txt"
"$MODBOX" gzip "$TMPDIR/gdc.txt" >/dev/null 2>&1
gdc_out=$("$MODBOX" gzip -dc "$TMPDIR/gdc.txt.gz" 2>/dev/null)
if [[ "$gdc_out" == "gzip -dc" && -f "$TMPDIR/gdc.txt.gz" && ! -f "$TMPDIR/gdc.txt" ]]; then
    pass "gzip -dc: stdout, keeps .gz, writes no output file"
else
    fail "gzip -dc: out=[$gdc_out] gz=$([[ -f "$TMPDIR/gdc.txt.gz" ]] && echo kept || echo gone) plain=$([[ -f "$TMPDIR/gdc.txt" ]] && echo written || echo absent)"
fi
# Without -c the input IS consumed, so the fix did not disable removal.
printf 'gzip -d removes\n' > "$TMPDIR/grm.txt"
"$MODBOX" gzip "$TMPDIR/grm.txt" >/dev/null 2>&1
"$MODBOX" gzip -d "$TMPDIR/grm.txt.gz" >/dev/null 2>&1
if [[ ! -f "$TMPDIR/grm.txt.gz" && -f "$TMPDIR/grm.txt" ]]; then
    pass "gzip -d (no -c): still removes the .gz"
else
    fail "gzip -d (no -c): failed to remove the .gz"
fi

echo "  ── -f overwrites existing decompressed output ──"
printf 'gunzip -f payload\n' > "$TMPDIR/gf.txt"
"$MODBOX" gzip "$TMPDIR/gf.txt" >/dev/null 2>&1
printf 'stale\n' > "$TMPDIR/gf.txt"
"$MODBOX" gunzip -f "$TMPDIR/gf.txt.gz" >/dev/null 2>&1
if [[ "$(cat "$TMPDIR/gf.txt")" == "gunzip -f payload" ]]; then
    pass "gunzip -f: overwrites existing output"
else
    fail "gunzip -f: overwrite failed"
fi

echo "  ── -q suppresses warnings (silent stderr) ──"
printf 'gunzip quiet\n' > "$TMPDIR/gq.txt"
"$MODBOX" gzip "$TMPDIR/gq.txt" >/dev/null 2>&1
cp "$TMPDIR/gq.txt.gz" "$TMPDIR/gq.1.gz"
cp "$TMPDIR/gq.txt.gz" "$TMPDIR/gq.2.gz"
cp "$TMPDIR/gq.txt.gz" "$TMPDIR/gq.3.gz"
"$MODBOX" gunzip -q -c "$TMPDIR/gq.1.gz" >/dev/null 2>/dev/null
status=$?
"$MODBOX" gunzip -q -c "$TMPDIR/gq.2.gz" >/dev/null 2>/tmp/gq.stderr
q_err=$(cat /tmp/gq.stderr)
rm -f /tmp/gq.stderr
if [[ $status -eq 0 && -z "$q_err" ]]; then
    pass "gunzip -q -c: quiet exit 0 with silent stderr"
else
    fail "gunzip -q -c: status=$status stderr=[$q_err]"
fi
rm -f "$TMPDIR/gq.1.gz" "$TMPDIR/gq.2.gz" "$TMPDIR/gq.3.gz"

echo "  ── non-gzip input error ──"
printf 'hello not gzip\n' > "$TMPDIR/gplain.txt"
"$MODBOX" gunzip "$TMPDIR/gplain.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then
    pass "gunzip: non-gzip input exits non-zero"
else
    fail "gunzip: non-gzip input should exit non-zero"
fi
assert_cmd_pat_stderr 'not in gzip format' gunzip "$TMPDIR/gplain.txt"

echo "  ── discoverable via modbox help ──"
assert_cmd_pat 'gunzip' help

# End of gunzip section.

if [[ $GZIP_HAS_SYS -eq 1 ]]; then
    printf 'interop payload 12345\n' > "$TMPDIR/io.txt"
    "$MODBOX" gzip "$TMPDIR/io.txt" >/dev/null 2>&1
    sys_out=$(gzip -dc "$TMPDIR/io.txt.gz" 2>/dev/null)
    if [[ "$sys_out" == "interop payload 12345" ]]; then pass "gzip: system gzip -d reads modbox .gz"; else fail "gzip: system gzip -d mismatch [$sys_out]"; fi
    printf 'reverse interop\n' > "$TMPDIR/rio.txt"
    gzip -c "$TMPDIR/rio.txt" > "$TMPDIR/rio.txt.gz" 2>/dev/null
    mb_out=$("$MODBOX" gzip -dc "$TMPDIR/rio.txt.gz" 2>/dev/null)
    if [[ "$mb_out" == "reverse interop" ]]; then pass "gzip: modbox -d reads system .gz"; else fail "gzip: modbox -d system .gz mismatch [$mb_out]"; fi
else
    echo "  SKIP interop (system gzip absent)"
fi
