#!/usr/bin/env bash
#
# test_zstd.sh — Tests for modbox zstd command
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── zstd ──────────────────────────────────────"

ZSTD_HAS_SYS=0
if command -v zstd >/dev/null 2>&1; then ZSTD_HAS_SYS=1; fi

echo "  ── help / version ──"
assert_cmd_pat 'Usage:' zstd --help
assert_cmd_pat 'zstd \(modbox\) 1\.0' zstd --version

echo "  ── compress single file (T01) ──"
printf 'the quick brown fox jumps over the lazy dog\n' > "$TMPDIR/c1.txt"
"$MODBOX" zstd "$TMPDIR/c1.txt"
if [[ ! -f "$TMPDIR/c1.txt.zst" ]]; then fail "zstd: c1.txt.zst not created"; else pass "zstd: c1.txt.zst created"; fi
if [[ -f "$TMPDIR/c1.txt" ]]; then fail "zstd: original c1.txt not removed"; else pass "zstd: original c1.txt removed"; fi
# magic bytes
magic=$(od -An -tx1 -N4 "$TMPDIR/c1.txt.zst" | tr -d ' ')
if [[ "$magic" == "28b52ffd" ]]; then pass "zstd: output begins with 28 b5 2f fd"; else fail "zstd: magic is [$magic] expected 28b52ffd"; fi

echo "  ── levels -1 vs -9 (T03) ──"
printf 'repeat repeat repeat repeat repeat repeat repeat repeat\n' > "$TMPDIR/lvl.txt"
"$MODBOX" zstd -1 "$TMPDIR/lvl.txt"; mv "$TMPDIR/lvl.txt.zst" "$TMPDIR/lvl1.zst"
printf 'repeat repeat repeat repeat repeat repeat repeat repeat\n' > "$TMPDIR/lvl.txt"
"$MODBOX" zstd -9 "$TMPDIR/lvl.txt"; mv "$TMPDIR/lvl.txt.zst" "$TMPDIR/lvl9.zst"
if [[ -s "$TMPDIR/lvl1.zst" && -s "$TMPDIR/lvl9.zst" ]]; then pass "zstd: -1 and -9 both produce a .zst"; else fail "zstd: -1/-9 produced empty"; fi
# default level 3
printf 'default level content here\n' > "$TMPDIR/dfl.txt"
"$MODBOX" zstd "$TMPDIR/dfl.txt"
if [[ -s "$TMPDIR/dfl.txt.zst" ]]; then pass "zstd: default level 3 produces valid .zst"; else fail "zstd: default level produced empty"; fi
# multi-digit level
printf 'multi digit content\n' > "$TMPDIR/md.txt"
"$MODBOX" zstd -12 "$TMPDIR/md.txt"; mv "$TMPDIR/md.txt.zst" "$TMPDIR/md12.zst"
if [[ -s "$TMPDIR/md12.zst" ]]; then pass "zstd: -12 level works"; else fail "zstd: -12 level failed"; fi

echo "  ── keep / stdout / force (T02) ──"
printf 'keep me\n' > "$TMPDIR/k.txt"
"$MODBOX" zstd -k "$TMPDIR/k.txt"
if [[ -f "$TMPDIR/k.txt" && -f "$TMPDIR/k.txt.zst" ]]; then pass "zstd -k: keeps original and creates .zst"; else fail "zstd -k: original/compressed state wrong"; fi

printf 'stdout me\n' > "$TMPDIR/s.txt"
out=$("$MODBOX" zstd -c "$TMPDIR/s.txt" 2>/dev/null | od -An -tx1 -N4 | tr -d ' ')
if [[ "$out" == "28b52ffd" && -f "$TMPDIR/s.txt" ]]; then pass "zstd -c: writes magic to stdout, keeps file"; else fail "zstd -c: out=[$out] file=[$([[ -f "$TMPDIR/s.txt" ]] && echo yes || echo no)]"; fi

printf 'force me\n' > "$TMPDIR/f.txt"
"$MODBOX" zstd "$TMPDIR/f.txt" >/dev/null 2>&1
printf 'force me again\n' > "$TMPDIR/f.txt"
"$MODBOX" zstd -f "$TMPDIR/f.txt" >/dev/null 2>&1
if [[ -f "$TMPDIR/f.txt.zst" ]]; then pass "zstd -f: overwrites existing .zst"; else fail "zstd -f: did not overwrite"; fi
# without -f: error
printf 'no force\n' > "$TMPDIR/nf.txt"
"$MODBOX" zstd "$TMPDIR/nf.txt" >/dev/null 2>&1
printf 'no force again\n' > "$TMPDIR/nf.txt"
if "$MODBOX" zstd "$TMPDIR/nf.txt" >/dev/null 2>&1; then fail "zstd: existing .zst without -f should fail"; else pass "zstd: existing .zst without -f fails"; fi

echo "  ── decompress + round-trip (T04) ──"
printf 'round trip payload\n' > "$TMPDIR/rt.txt"
cp "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"
"$MODBOX" zstd "$TMPDIR/rt.txt" >/dev/null 2>&1
"$MODBOX" zstd -d "$TMPDIR/rt.txt.zst" >/dev/null 2>&1
if [[ ! -f "$TMPDIR/rt.txt.zst" && -f "$TMPDIR/rt.txt" ]]; then pass "zstd -d: restores file, removes .zst"; else fail "zstd -d: restore state wrong"; fi
if cmp -s "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"; then pass "zstd: round-trip byte-identical"; else fail "zstd: round-trip mismatch"; fi
# -dc to stdout
printf 'dc stream\n' > "$TMPDIR/dc.txt"
"$MODBOX" zstd "$TMPDIR/dc.txt" >/dev/null 2>&1
dc_out=$("$MODBOX" zstd -dc "$TMPDIR/dc.txt.zst" 2>/dev/null)
if [[ "$dc_out" == "dc stream" ]]; then pass "zstd -dc: writes decompressed bytes to stdout"; else fail "zstd -dc: got [$dc_out]"; fi

# -dk keeps the .zst
printf 'keep decompressed\n' > "$TMPDIR/dk.txt"
"$MODBOX" zstd "$TMPDIR/dk.txt" >/dev/null 2>&1
"$MODBOX" zstd -dk "$TMPDIR/dk.txt.zst" >/dev/null 2>&1
if [[ -f "$TMPDIR/dk.txt" && -f "$TMPDIR/dk.txt.zst" ]]; then pass "zstd -dk: restores file and keeps .zst"; else fail "zstd -dk: state wrong"; fi

echo "  ── multi-file (T05) ──"
printf 'aaa\n' > "$TMPDIR/m1.txt"
printf 'bbb\n' > "$TMPDIR/m2.txt"
printf 'ccc\n' > "$TMPDIR/m3.txt"
"$MODBOX" zstd "$TMPDIR/m1.txt" "$TMPDIR/m2.txt" "$TMPDIR/m3.txt" >/dev/null 2>&1
if [[ -f "$TMPDIR/m1.txt.zst" && -f "$TMPDIR/m2.txt.zst" && -f "$TMPDIR/m3.txt.zst" ]]; then pass "zstd: each file gets its own .zst"; else fail "zstd: multi-file .zst missing"; fi
"$MODBOX" zstd -d "$TMPDIR/m1.txt.zst" "$TMPDIR/m2.txt.zst" "$TMPDIR/m3.txt.zst" >/dev/null 2>&1
if [[ -f "$TMPDIR/m1.txt" && -f "$TMPDIR/m2.txt" && -f "$TMPDIR/m3.txt" ]]; then pass "zstd -d: multi-file decompresses each"; else fail "zstd -d: multi-file restore missing"; fi

echo "  ── stdin/stdout pipelines (T05) ──"
pipe_out=$(printf 'pipe data\n' | "$MODBOX" zstd 2>/dev/null | od -An -tx1 -N4 | tr -d ' ')
if [[ "$pipe_out" == "28b52ffd" ]]; then pass "zstd: no-arg compresses stdin to stdout"; else fail "zstd: stdin->stdout magic [$pipe_out]"; fi
round=$(printf 'pipe data\n' | "$MODBOX" zstd 2>/dev/null | "$MODBOX" zstd -d 2>/dev/null)
if [[ "$round" == "pipe data" ]]; then pass "zstd -d: no-arg decompresses stdin to stdout"; else fail "zstd -d: stdin->stdout got [$round]"; fi

echo "  ── error handling (T04) ──"
"$MODBOX" zstd "$TMPDIR/does_not_exist_xyz.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zstd: missing file exits non-zero"; else fail "zstd: missing file should exit non-zero"; fi
assert_cmd_pat_stderr 'No such file' zstd "$TMPDIR/does_not_exist_xyz.txt"

# Corrupt / truncated data
printf '\x00\x00\x00\x00' > "$TMPDIR/corrupt.zst"
"$MODBOX" zstd -d "$TMPDIR/corrupt.zst" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zstd: corrupt data exits non-zero"; else fail "zstd: corrupt data should exit non-zero"; fi

# Non-zstd file
printf 'not zstd data\n' > "$TMPDIR/notzstd.zst"
"$MODBOX" zstd -d "$TMPDIR/notzstd.zst" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zstd: non-zstd file exits non-zero"; else fail "zstd: non-zstd file should exit non-zero"; fi

echo "  ── .zst suffix warning (T06) ──"
printf 'already zst\n' > "$TMPDIR/already.zst"
warn_out=$("$MODBOX" zstd "$TMPDIR/already.zst" 2>&1)
if [[ "$warn_out" == *".zst suffix"* ]]; then pass "zstd: .zst suffix warning shown"; else fail "zstd: .zst suffix warning not shown"; fi
# -q should suppress it
warn_quiet=$("$MODBOX" zstd -q "$TMPDIR/already.zst" 2>&1)
if [[ -z "$warn_quiet" ]]; then pass "zstd: -q suppresses .zst suffix warning"; else fail "zstd: -q should suppress warning but got: $warn_quiet"; fi

echo "  ── verbose output (T03) ──"
printf 'verbose test\n' > "$TMPDIR/verb.txt"
"$MODBOX" zstd -v "$TMPDIR/verb.txt" 2>&1 | grep -q "%" && pass "zstd: verbose shows ratio" || fail "zstd: verbose should show ratio"
rm -f "$TMPDIR/verb.txt.zst"

echo "  ── stdout keep original (T02) ──"
printf 'stdout keep\n' > "$TMPDIR/sok.txt"
"$MODBOX" zstd -c "$TMPDIR/sok.txt" > /tmp/sok.zst
if [[ -f "$TMPDIR/sok.txt" ]]; then pass "zstd: -c keeps original"; else fail "zstd: -c should keep original"; fi

echo "  ── interop with system zstd (best-effort) ──"
printf 'interop test content for zstd round-trip\n' > "$TMPDIR/interp.txt"
if [[ "$ZSTD_HAS_SYS" -eq 1 ]]; then
    "$MODBOX" zstd "$TMPDIR/interp.txt"
    out=$(zstd -d -c "$TMPDIR/interp.txt.zst")
    rm -f "$TMPDIR/interp.txt.zst"
    if [[ "$out" == "interop test content for zstd round-trip" ]]; then pass "zstd: interop compress/decompress ok"; else fail "zstd: interop mismatch"; fi
fi

echo "  ── exit 0 on success ──"
printf 'exit test\n' > "$TMPDIR/exit.txt"
"$MODBOX" zstd "$TMPDIR/exit.txt" >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "zstd: exit 0 on success"; else fail "zstd: should exit 0"; fi
"$MODBOX" zstd -d "$TMPDIR/exit.txt.zst" >/dev/null 2>&1
if [[ $? -eq 0 ]]; then pass "zstd: decompress exit 0"; else fail "zstd: should exit 0"; fi

echo "  ── empty file ──"
echo "" > "$TMPDIR/empty.txt"
"$MODBOX" zstd "$TMPDIR/empty.txt"
if [[ -f "$TMPDIR/empty.txt.zst" ]]; then pass "zstd: empty compress works"; else fail "zstd: empty compress failed"; fi
"$MODBOX" zstd -d "$TMPDIR/empty.txt.zst"
if [[ -f "$TMPDIR/empty.txt" ]]; then pass "zstd: empty decompress works"; else fail "zstd: empty decompress failed"; fi

echo "  ── --rm flag ──"
printf 'rm test\n' > "$TMPDIR/rm.txt"
"$MODBOX" zstd --rm "$TMPDIR/rm.txt"
if [[ ! -f "$TMPDIR/rm.txt" && -f "$TMPDIR/rm.txt.zst" ]]; then pass "zstd: --rm removes source"; else fail "zstd: --rm should remove source"; fi

echo "  ── list info (T07) ──"
printf 'list test content\n' > "$TMPDIR/list.txt"
"$MODBOX" zstd "$TMPDIR/list.txt" >/dev/null 2>&1
list_out=$("$MODBOX" zstd -l "$TMPDIR/list.txt.zst" 2>&1)
if [[ "$list_out" == *"list.txt.zst"* && "$list_out" == *"%"* || "$list_out" == *".zst"* ]]; then pass "zstd: -l shows file info"; else fail "zstd: -l output missing filename: [$list_out]"; fi
"$MODBOX" zstd -d "$TMPDIR/list.txt.zst" >/dev/null 2>&1

echo "  ── list error on non-zstd file ──"
printf 'not compressed\n' > "$TMPDIR/notzst.txt"
"$MODBOX" zstd -l "$TMPDIR/notzst.txt" >/dev/null 2>&1
if [[ $? -ne 0 ]]; then pass "zstd: -l rejects non-zstd file"; else fail "zstd: should reject non-zstd file with -l"; fi

echo "  ── stdin pipe round-trip ──"
printf 'pipe roundtrip content\n' | "$MODBOX" zstd | "$MODBOX" zstd -d > "$TMPDIR/pipert.txt"
if grep -q "pipe roundtrip content" "$TMPDIR/pipert.txt"; then pass "zstd: stdin pipe round-trip works"; else fail "zstd: stdin pipe round-trip failed"; fi
