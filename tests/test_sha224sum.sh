SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── sha224sum ──────────────────────────────────"

# Create test files
printf 'hello world\n' > "$TMPDIR"/sha224_test.txt
printf '' > "$TMPDIR"/sha224_empty.txt

echo "  ── basic file checksum ──"
assert_cmd_pat '^[a-f0-9]{56}  '"$TMPDIR"'/sha224_test.txt$' sha224sum "$TMPDIR"/sha224_test.txt

echo "  ── empty file checksum ──"
assert_cmd_pat '^[a-f0-9]{56}  '"$TMPDIR"'/sha224_empty.txt$' sha224sum "$TMPDIR"/sha224_empty.txt

echo "  ── stdin (no file) ──"
assert_cmd_pat '^[a-f0-9]{56}  -$' sha224sum <<<"hello world"

echo "  ── stdin (explicit -) ──"
assert_cmd_pat '^[a-f0-9]{56}  -$' sha224sum - <<<"hello world"

echo "  ── binary mode (-b) ──"
assert_cmd_pat '^[a-f0-9]{56} \*'"$TMPDIR"'/sha224_test.txt$' sha224sum -b "$TMPDIR"/sha224_test.txt

echo "  ── matches system sha224sum ──"
if command -v sha224sum &>/dev/null; then
    real_sha=$(sha224sum "$TMPDIR"/sha224_test.txt | cut -d' ' -f1)
    assert_cmd_pat "^$real_sha  " sha224sum "$TMPDIR"/sha224_test.txt
else
    echo "  SKIP (system sha224sum not available)"
fi

echo "  ── check mode (-c) with valid checksum ──"
if command -v sha224sum &>/dev/null; then
    real_sha=$(sha224sum "$TMPDIR"/sha224_test.txt | cut -d' ' -f1)
    printf '%s  %s\n' "$real_sha" "$TMPDIR"/sha224_test.txt > "$TMPDIR"/sha224_checksums.txt
    assert_cmd_pat 'OK$' sha224sum -c "$TMPDIR"/sha224_checksums.txt
else
    echo "  SKIP (system sha224sum not available)"
fi

echo "  ── check mode with invalid checksum ──"
printf '%056d  %s\n' 0 "$TMPDIR"/sha224_test.txt > "$TMPDIR"/sha224_bad_checksums.txt
assert_cmd_pat 'FAILED' sha224sum -c "$TMPDIR"/sha224_bad_checksums.txt

echo "  ── --tag format ──"
assert_cmd_pat '^SHA224 \('"$TMPDIR"'/sha224_test.txt\) = [a-f0-9]{56}' sha224sum --tag "$TMPDIR"/sha224_test.txt

echo "  ── help ──"
assert_cmd_pat 'Usage:' sha224sum --help

echo "  ── error: nonexistent file ──"
assert_cmd_pat_stderr 'No such file' sha224sum "$TMPDIR"/sha224_nonexistent.txt
