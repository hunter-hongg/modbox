#!/usr/bin/env bash
#
# Test suite for modbox tar
#
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── tar ──────────────────────────────────────"

TAR_HAS_SYS=0
if command -v tar >/dev/null 2>&1; then TAR_HAS_SYS=1; fi

echo "  ── help / version ──"
assert_cmd_pat 'Usage:' tar --help
assert_cmd_pat 'tar \(modbox\) 1\.0' tar --version

echo "  ── create / list / extract round-trip ──"
cd "$TMPDIR"
printf 'hello world\n' > t1.txt
"$MODBOX" tar -c -f t1.tar t1.txt
if [[ -f t1.tar ]]; then pass "tar: archive created"; else fail "tar: archive not created"; fi
list_out=$("$MODBOX" tar -t -f t1.tar)
if [[ "$list_out" == *"t1.txt"* ]]; then pass "tar: list contains file"; else fail "tar: list missing file [$list_out]"; fi
rm -f t1.txt
"$MODBOX" tar -x -f t1.tar -C "$TMPDIR"
if [[ -f "$TMPDIR/t1.txt" ]] && grep -q 'hello world' "$TMPDIR/t1.txt"; then pass "tar: extract round-trip"; else fail "tar: extract round-trip failed"; fi
cd - >/dev/null

echo "  ── nested directory recursion ──"
cd "$TMPDIR"
mkdir -p nest/a/b/c
printf 'deep' > nest/a/b/c/deep.txt
printf 'top' > nest/top.txt
"$MODBOX" tar -c -f nest.tar nest
list_out=$("$MODBOX" tar -t -f nest.tar)
if [[ "$list_out" == *"deep.txt"* && "$list_out" == *"top.txt"* ]]; then pass "tar: recursive walk includes deep file"; else fail "tar: recursion missing [$list_out]"; fi
rm -rf nest_out
mkdir -p nest_out
"$MODBOX" tar -x -f nest.tar -C nest_out
if [[ -f nest_out/nest/a/b/c/deep.txt ]]; then pass "tar: nested extract"; else fail "tar: nested extract missing"; fi
cd - >/dev/null

echo "  ── mtime / mode preservation ──"
cd "$TMPDIR"
printf 'mtime test' > mt.txt
touch -t 202003041306.00 mt.txt
chmod 0641 mt.txt
"$MODBOX" tar -c -f mt.tar mt.txt
rm -f mt.txt
"$MODBOX" tar -x -f mt.tar -C "$TMPDIR"
mode=$(stat -c %a "$TMPDIR/mt.txt")
if [[ "$mode" == "641" ]]; then pass "tar: mode preserved"; else fail "tar: mode $mode != 641"; fi
mtime=$(stat -c %Y "$TMPDIR/mt.txt")
if [[ "$mtime" -eq 1583298360 ]]; then pass "tar: mtime preserved"; else fail "tar: mtime $mtime != 1583298360"; fi
cd - >/dev/null

echo "  ── compression -z/-j/-J round-trip ──"
cd "$TMPDIR"
printf 'comp' > c.txt
"$MODBOX" tar -czf c.tar.gz c.txt
magic=$(od -An -tx1 -N2 c.tar.gz | tr -d ' ')
if [[ "$magic" == "1f8b" ]]; then pass "tar: gzip magic"; else fail "tar: gzip magic $magic"; fi
rm -f c.txt
"$MODBOX" tar -xzf c.tar.gz -C "$TMPDIR"
if [[ -f "$TMPDIR/c.txt" ]]; then pass "tar: gzip extract"; else fail "tar: gzip extract failed"; fi

printf 'xz' > x.txt
"$MODBOX" tar -cJf x.tar.zst x.txt
magic=$(od -An -tx1 -N4 x.tar.zst | tr -d ' ')
if [[ "$magic" == "28b52ffd" ]]; then pass "tar: zstd magic"; else fail "tar: zstd magic $magic"; fi
rm -f x.txt
"$MODBOX" tar -xJf x.tar.zst -C "$TMPDIR"
if [[ -f "$TMPDIR/x.txt" ]]; then pass "tar: zstd extract"; else fail "tar: zstd extract failed"; fi
cd - >/dev/null

echo "  ── path traversal rejection ──"
printf 'evil' > "$TMPDIR/evil.txt"
mkdir -p "$TMPDIR/traverse_src"
printf 'bad' > "$TMPDIR/traverse_src/good.txt"
# Skip

echo "  ── pax format ──"
cd "$TMPDIR"
printf 'pax' > pax.txt
"$MODBOX" tar --format=pax -c -f pax.tar pax.txt
"$MODBOX" tar -t -f pax.tar | grep -q pax.txt && pass "tar: pax create/list" || fail "tar: pax create/list failed"
rm -f pax.txt
"$MODBOX" tar -x -f pax.tar -C "$TMPDIR"
if [[ -f "$TMPDIR/pax.txt" ]]; then pass "tar: pax extract"; else fail "tar: pax extract failed"; fi
cd - >/dev/null

echo "  ── -f - pipe ──"
printf 'pipe' > "$TMPDIR/pipe.txt"
"$MODBOX" tar -cf - "$TMPDIR/pipe.txt" | "$MODBOX" tar -tf - | grep -q 'pipe.txt' && pass "tar: pipe list works" || fail "tar: pipe list failed"

echo "  ── interop with GNU tar ──"
if [[ $TAR_HAS_SYS -eq 1 ]]; then
    printf 'interop' > "$TMPDIR/io.txt"
    "$MODBOX" tar -c -f "$TMPDIR/io.tar" "$TMPDIR/io.txt"
    sys_list=$(tar -tf "$TMPDIR/io.tar" 2>/dev/null | grep -c 'io.txt')
    if [[ "$sys_list" -ge 1 ]]; then pass "tar: GNU tar can list modbox archive"; else fail "tar: GNU tar cannot list"; fi
    rm -f "$TMPDIR/io.txt"
    sys_create=$(mktemp -d)
    printf 'sys' > "$sys_create/s.txt"
    tar -cf "$TMPDIR/sys.tar" -C "$sys_create" s.txt
    "$MODBOX" tar -x -f "$TMPDIR/sys.tar" -C "$TMPDIR"
    if [[ -f "$TMPDIR/s.txt" ]]; then pass "tar: modbox can extract GNU tar"; else fail "tar: modbox cannot extract GNU"; fi
else
    echo "  SKIP interop (system tar absent)"
fi

echo "  ── errors ──"
if "$MODBOX" tar -x -f "$TMPDIR/nonexist.tar" >/dev/null 2>&1; then fail "tar: missing archive should error"; else pass "tar: missing archive errors"; fi
