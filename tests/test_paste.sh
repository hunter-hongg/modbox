SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── paste ─────────────────────────────────────"

printf 'a\nb\nc\n' > "$TMPDIR"/paste_f1.txt
printf '1\n2\n3\n' > "$TMPDIR"/paste_f2.txt
printf 'x\ny\nz\n' > "$TMPDIR"/paste_f3.txt
printf 'single\n' > "$TMPDIR"/paste_single.txt
printf '' > "$TMPDIR"/paste_empty.txt

echo "  ── basic parallel ──"
assert_cmd "$(printf 'a\t1\nb\t2\nc\t3\n')" paste "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── three files ──"
assert_cmd "$(printf 'a\t1\tx\nb\t2\ty\nc\t3\tz\n')" paste "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── serial ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\n')" paste -s "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── serial three files ──"
assert_cmd "$(printf 'a\tb\tc\n1\t2\t3\nx\ty\tz\n')" paste -s "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── custom delimiter ──"
assert_cmd "$(printf 'a:1\nb:2\nc:3\n')" paste -d: "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── multi-char delimiter cycle ──"
assert_cmd "$(printf 'a:1,x\nb:2,y\nc:3,z\n')" paste -d ':,' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── pipe delimiter (special char) ──"
assert_cmd "$(printf 'a|1\nb|2\nc|3\n')" paste -d '|' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── stdin via - ──"
assert_cmd "$(printf 'hello\ta\nworld\tb\n\tc\n')" paste - "$TMPDIR"/paste_f1.txt <<<"$(printf 'hello\nworld')"

echo "  ── no files reads stdin ──"
assert_cmd "$(printf 'one\ntwo\nthree')" paste <<<"$(printf 'one\ntwo\nthree')"

echo "  ── single file (passthrough) ──"
assert_cmd "$(printf 'a\nb\nc\n')" paste "$TMPDIR"/paste_f1.txt

echo "  ── single file serial ──"
assert_cmd "$(printf 'a\tb\tc')" paste -s "$TMPDIR"/paste_f1.txt

echo "  ── empty file ──"
assert_cmd "" paste "$TMPDIR"/paste_empty.txt

echo "  ── empty file parallel ──"
assert_cmd "$(printf '\ta\n\tb\n\tc')" paste "$TMPDIR"/paste_empty.txt "$TMPDIR"/paste_f1.txt

echo "  ── uneven line counts ──"
printf 'p\nq\n' > "$TMPDIR"/paste_short.txt
assert_cmd "$(printf 'a\tp\nb\tq\nc\t\n')" paste "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_short.txt

echo "  ── serial uneven lines ──"
printf 'a\nb\n' > "$TMPDIR"/paste_ser_short.txt
assert_cmd "$(printf 'a\tb\n1\t2\t3')" paste -s "$TMPDIR"/paste_ser_short.txt "$TMPDIR"/paste_f2.txt

echo "  ── zero-terminated ──"
printf 'a\nb\n' | "$MODBOX" paste -z - "$TMPDIR"/paste_single.txt > "$TMPDIR"/paste_zout.bin 2>/dev/null || true
if od -c "$TMPDIR"/paste_zout.bin | grep -q '\\0'; then
    pass "paste -z → NUL delimited"
else
    fail "paste -z — expected NUL in output"
fi

echo "  ── zero-terminated output structure ──"
printf 'a\nb\n' > "$TMPDIR"/paste_zf1.txt
printf '1\n2\n' > "$TMPDIR"/paste_zf2.txt
"$MODBOX" paste -z "$TMPDIR"/paste_zf1.txt "$TMPDIR"/paste_zf2.txt > "$TMPDIR"/paste_zout2.bin 2>/dev/null || true
# Expect 2 NULs: a<tab>1<NUL>b<tab>2<NUL>
field_count=$(tr -d -c '\0' < "$TMPDIR"/paste_zout2.bin | wc -c)
if [ "$field_count" -eq 2 ]; then
    pass "paste -z two files → 2 NUL delimiters"
else
    fail "paste -z two files — expected 2 NUL chars, got $field_count"
fi

echo "  ── nonexistent file ──"
assert_cmd_pat_stderr "No such file" paste "$TMPDIR"/paste_nonexistent.txt "$TMPDIR"/paste_f1.txt

echo "  ── multiple nonexistent files ──"
assert_cmd_pat_stderr "No such file" paste "$TMPDIR"/paste_nonexistent.txt "$TMPDIR"/paste_f2.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' paste --help

echo "  ── version ──"
assert_cmd_pat 'modbox' paste --version

echo "  ── -d empty delimiter ──"
assert_cmd "$(printf 'a1\nb2\nc3\n')" paste -d '' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── -d empty delimiter with three files ──"
assert_cmd "$(printf 'a1x\nb2y\nc3z\n')" paste -d '' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt "$TMPDIR"/paste_f3.txt

echo "  ── serial with custom delimiter ──"
assert_cmd "$(printf 'a:b:c\n1:2:3\n')" paste -s -d: "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt

echo "  ── serial with multi-char delimiter cycle ──"
assert_cmd "$(printf 'a,b-c\n1,2-3\n')" paste -s -d ',-' "$TMPDIR"/paste_f1.txt "$TMPDIR"/paste_f2.txt
