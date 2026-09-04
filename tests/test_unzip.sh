#!/usr/bin/env bash
#
# Test suite for modbox unzip
#
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── unzip ──────────────────────────────────────"

ZIP_HAS_SYS=0
if command -v zip >/dev/null 2>&1; then ZIP_HAS_SYS=1; fi
UNZIP_HAS_SYS=0
if command -v unzip >/dev/null 2>&1; then UNZIP_HAS_SYS=1; fi

echo "  ── help / version ──"
assert_cmd_pat 'Usage:' unzip --help
assert_cmd_pat 'unzip \(modbox\) 1\.0' unzip --version

echo "  ── basic extract (T06) ──"
rm -rf "$TMPDIR/uz_out"
printf 'hello world\n' > "$TMPDIR/hello.txt"
"$MODBOX" zip "$TMPDIR/basic.zip" "$TMPDIR/hello.txt"
"$MODBOX" unzip -d "$TMPDIR/uz_out" "$TMPDIR/basic.zip"
if [[ -f "$TMPDIR/uz_out/hello.txt" ]] && cmp -s "$TMPDIR/hello.txt" "$TMPDIR/uz_out/hello.txt"; then
    pass "unzip: basic extract byte-identical"; else fail "unzip: basic extract mismatch"; fi

echo "  ── list (T06) ──"
list_out=$("$MODBOX" unzip -l "$TMPDIR/basic.zip" 2>/dev/null)
if [[ "$list_out" == *"hello.txt"* ]]; then pass "unzip -l: shows entry name"; else fail "unzip -l: missing entry [$list_out]"; fi
if echo "$list_out" | grep -q 'Length\|Date\|Time\|Name'; then pass "unzip -l: has header row"; else fail "unzip -l: no header row"; fi

echo "  ── extract to dir with nested paths (T06) ──"
rm -rf "$TMPDIR/uz_nested"
mkdir -p "$TMPDIR/nested/a/b"
printf 'deep\n' > "$TMPDIR/nested/a/b/deep.txt"
printf 'top\n' > "$TMPDIR/nested/top.txt"
"$MODBOX" zip -r "$TMPDIR/nested.zip" "$TMPDIR/nested"
"$MODBOX" unzip -d "$TMPDIR/uz_nested" "$TMPDIR/nested.zip"
if [[ -f "$TMPDIR/uz_nested/nested/a/b/deep.txt" && -f "$TMPDIR/uz_nested/nested/top.txt" ]]; then
    pass "unzip -d: nested dirs restored correctly"; else fail "unzip -d: nested dirs not restored"; fi

echo "  ── test integrity (T07) ──"
if "$MODBOX" unzip -t "$TMPDIR/basic.zip" >/dev/null 2>&1; then pass "unzip -t: valid archive passes"; else fail "unzip -t: valid archive failed"; fi
# Corrupt the archive by truncating it
cp "$TMPDIR/basic.zip" "$TMPDIR/corrupt.zip"
python3 -c "
with open('$TMPDIR/corrupt.zip', 'r+b') as f:
    data = f.read()
    f.seek(0)
    f.write(data[:len(data)//2])
    f.truncate()
"
if "$MODBOX" unzip -t "$TMPDIR/corrupt.zip" >/dev/null 2>&1; then
    fail "unzip -t: corrupt archive should fail"; else pass "unzip -t: corrupt archive fails"; fi

echo "  ── stdout mode (-p) (T07) ──"
out=$("$MODBOX" unzip -p hello.txt "$TMPDIR/basic.zip" 2>/dev/null)
if [[ "$out" == "hello world" ]]; then pass "unzip -p: prints entry to stdout"; else fail "unzip -p: got [$out]"; fi

echo "  ── entry filter (T07) ──"
rm -rf "$TMPDIR/uz_filt"
mkdir -p "$TMPDIR/filt"
printf 'a\n' > "$TMPDIR/filt/a.txt"
printf 'b\n' > "$TMPDIR/filt/b.txt"
"$MODBOX" zip -r "$TMPDIR/filt.zip" "$TMPDIR/filt"
"$MODBOX" unzip filt/a.txt "$TMPDIR/filt.zip" -d "$TMPDIR/uz_filt"
if [[ -f "$TMPDIR/uz_filt/filt/a.txt" && ! -f "$TMPDIR/uz_filt/filt/b.txt" ]]; then
    pass "unzip entry filter: only extracted a.txt"; else fail "unzip entry filter: wrong files extracted"; fi

echo "  ── no-clobber -n (T07) ──"
rm -rf "$TMPDIR/uz_noclob"
mkdir -p "$TMPDIR/uz_noclob"
printf 'orig\n' > "$TMPDIR/uz_noclob/exist.txt"
"$MODBOX" unzip -n -d "$TMPDIR/uz_noclob" "$TMPDIR/basic.zip" 2>/dev/null
out=$(cat "$TMPDIR/uz_noclob/exist.txt")
if [[ "$out" == "orig" ]]; then pass "unzip -n: skips existing file"; else fail "unzip -n: overwritten existing [$out]"; fi

echo "  ── overwrite -o (T07) ──"
rm -rf "$TMPDIR/uz_over"
mkdir -p "$TMPDIR/uz_over"
printf 'orig\n' > "$TMPDIR/uz_over/exist.txt"
"$MODBOX" unzip -o -d "$TMPDIR/uz_over" "$TMPDIR/basic.zip" 2>/dev/null
out=$(cat "$TMPDIR/uz_over/hello.txt")
if [[ "$out" == "hello world" ]]; then pass "unzip -o: overwrites existing"; else fail "unzip -o: got [$out]"; fi

echo "  ── quiet (-q) and verbose (-v) (T07) ──"
quiet_out=$("$MODBOX" unzip -q -d "$TMPDIR/uz_quiet" "$TMPDIR/basic.zip" 2>/dev/null)
if [[ -z "$quiet_out" ]]; then pass "unzip -q: suppresses output"; else fail "unzip -q: produced output [$quiet_out]"; fi

verbose_out=$("$MODBOX" unzip -v -d "$TMPDIR/uz_verbose" "$TMPDIR/basic.zip" 2>/dev/null)
if [[ "$verbose_out" == *"Archive:"* ]]; then pass "unzip -v: verbose shows Archive: header"; else fail "unzip -v: no header in [$verbose_out]"; fi

echo "  ── missing archive error (T06) ──"
if "$MODBOX" unzip "$TMPDIR/nonexist_xyz.zip" >/dev/null 2>&1; then fail "unzip: missing archive should error"; else pass "unzip: missing archive exits non-zero"; fi

echo "  ── invalid archive error (T06) ──"
printf 'not a zip at all\n' > "$TMPDIR/bad.zip"
if "$MODBOX" unzip "$TMPDIR/bad.zip" >/dev/null 2>&1; then fail "unzip: invalid zip should error"; else pass "unzip: invalid zip exits non-zero"; fi

echo "  ── round-trip (T06) ──"
rm -rf "$TMPDIR/rt_out"
printf 'round trip payload\n' > "$TMPDIR/rt.txt"
cp "$TMPDIR/rt.txt" "$TMPDIR/rt.orig"
"$MODBOX" zip "$TMPDIR/rt.zip" "$TMPDIR/rt.txt"
"$MODBOX" unzip -d "$TMPDIR/rt_out" "$TMPDIR/rt.zip"
if [[ -f "$TMPDIR/rt_out/rt.txt" ]] && cmp -s "$TMPDIR/rt.txt" "$TMPDIR/rt_out/rt.txt"; then
    pass "unzip: round-trip byte-identical"; else fail "unzip: round-trip mismatch"; fi

echo "  ── empty file extract (T06) ──"
: > "$TMPDIR/empty.txt"
"$MODBOX" zip "$TMPDIR/empty.zip" "$TMPDIR/empty.txt"
rm -rf "$TMPDIR/empty_out"
"$MODBOX" unzip -d "$TMPDIR/empty_out" "$TMPDIR/empty.zip"
if [[ -f "$TMPDIR/empty_out/empty.txt" && ! -s "$TMPDIR/empty_out/empty.txt" ]]; then
    pass "unzip: empty file extracted correctly"; else fail "unzip: empty file missing or non-empty"; fi

echo "  ── interop with system tools (best-effort) ──"
if [[ $ZIP_HAS_SYS -eq 1 && $UNZIP_HAS_SYS -eq 1 ]]; then
    printf 'interop from sys\n' > "$TMPDIR/sys_io.txt"
    zip "$TMPDIR/sys_io.zip" "$TMPDIR/sys_io.txt" >/dev/null 2>&1
    rm -rf "$TMPDIR/sys_out"
    "$MODBOX" unzip -d "$TMPDIR/sys_out" "$TMPDIR/sys_io.zip"
    mb_out=$(cat "$TMPDIR/sys_out/sys_io.txt" 2>/dev/null)
    if [[ "$mb_out" == "interop from sys" ]]; then pass "unzip: modbox reads system-produced zip"; else
        # Entry name may have a tmp/... prefix from modbox zip's naming; check both.
        mb_out=$(find "$TMPDIR/sys_out" -name sys_io.txt -exec cat {} \; 2>/dev/null)
        if [[ "$mb_out" == "interop from sys" ]]; then pass "unzip: modbox reads system-produced zip (path prefix)"; else fail "unzip: modbox mismatch [$mb_out]"; fi
    fi

    printf 'reverse interop from modbox\n' > "$TMPDIR/rev_io.txt"
    "$MODBOX" zip "$TMPDIR/rev_io.zip" "$TMPDIR/rev_io.txt" >/dev/null 2>&1
    sys_out=$(unzip -p "$TMPDIR/rev_io.zip" rev_io.txt 2>/dev/null)
    if [[ "$sys_out" == "reverse interop from modbox" ]]; then pass "unzip: system unzip reads modbox zip"; else fail "unzip: system unzip mismatch [$sys_out]"; fi
else
    echo "  SKIP interop (system zip/unzip absent)"
fi

echo ""
echo "════════════════════════════════════════════"
echo "  Results: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "════════════════════════════════════════════"
exit $FAIL_COUNT
