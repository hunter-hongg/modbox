SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── uniq ──────────────────────────────────────"

printf 'apple\napple\nbanana\ncherry\ncherry\ncherry\n' > "$TMPDIR"/uniq_basic
printf 'a\nb\nc\n' > "$TMPDIR"/uniq_unique
printf 'a\na\nb\nb\n' > "$TMPDIR"/uniq_all_dup

echo "  ── basic uniq (merge adjacent duplicates)"
assert_cmd "$(printf 'apple\nbanana\ncherry\n')" uniq "$TMPDIR"/uniq_basic

echo "  ── -c: count"
assert_cmd "$(printf '      2 apple\n      1 banana\n      3 cherry\n')" uniq -c "$TMPDIR"/uniq_basic

echo "  ── -d: repeated only"
assert_cmd "$(printf 'apple\ncherry\n')" uniq -d "$TMPDIR"/uniq_basic

echo "  ── -D: all repeated"
assert_cmd "$(printf 'apple\napple\ncherry\ncherry\ncherry\n')" uniq -D "$TMPDIR"/uniq_basic

echo "  ── -u: unique only"
assert_cmd "$(printf 'banana\n')" uniq -u "$TMPDIR"/uniq_basic

echo "  ── -i: ignore case"
printf 'Apple\nAPPLE\nbanana\nBanana\n' > "$TMPDIR"/uniq_case
assert_cmd "$(printf 'Apple\nbanana\n')" uniq -i "$TMPDIR"/uniq_case

echo "  ── -f: skip fields"
printf 'a x\nb y\na z\n' > "$TMPDIR"/uniq_fields
# -f 1 skips first field when comparing: x, y, z are all different
assert_cmd "$(printf 'a x\nb y\na z\n')" uniq -f 1 "$TMPDIR"/uniq_fields

echo "  ── -s: skip chars"
printf 'xxa\nyya\nzzb\n' > "$TMPDIR"/uniq_skipchars
assert_cmd "$(printf 'xxa\nzzb\n')" uniq -s 2 "$TMPDIR"/uniq_skipchars

echo "  ── -w: check chars"
printf 'apple\napples\nbanana\n' > "$TMPDIR"/uniq_checkchars
assert_cmd "$(printf 'apple\nbanana\n')" uniq -w 3 "$TMPDIR"/uniq_checkchars

echo "  ── input/output files"
printf 'a\na\nb\n' > "$TMPDIR"/uniq_inout
assert_cmd "" uniq "$TMPDIR"/uniq_inout "$TMPDIR"/uniq_out
assert_cmd "$(printf 'a\nb\n')" cat "$TMPDIR"/uniq_out

echo "  ── stdin"
assert_cmd "$(printf 'a\nb\n')" uniq <<<"$(printf 'a\na\nb\n')"

echo "  ── help"
assert_cmd_pat 'Usage:' uniq --help

echo "  ── error: nonexistent input"
assert_cmd_pat_stderr 'No such file' uniq "$TMPDIR"/uniq_nonexistent
