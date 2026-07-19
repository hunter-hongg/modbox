SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── date ─────────────────────────────────────"

echo "  ── default output format ──"
assert_cmd_pat '^[A-Z][a-z]{2} [A-Z][a-z]{2} [ 0-9][0-9] [0-9]{2}:[0-9]{2}:[0-9]{2}' date

echo "  ── +%Y-%m-%d ──"
assert_cmd "2024-01-02" date +%Y-%m-%d -d "2024-01-02 03:04:05"

echo "  ── +%H:%M:%S ──"
assert_cmd "03:04:05" date +%H:%M:%S -d "2024-01-02 03:04:05"

echo "  ── +%F %T ──"
assert_cmd "2024-01-02 03:04:05" date +"%F %T" -d "2024-01-02 03:04:05"

echo "  ── UTC via -u ──"
assert_cmd "2024-01-02 03:04:05" date -u +%Y-%m-%d" "%H:%M:%S -d "2024-01-02 03:04:05"

echo "  ── -I (date only) ──"
assert_cmd "2024-01-02" date -I -d "2024-01-02 03:04:05"

echo "  ── -Iseconds ──"
assert_cmd_pat '^2024-01-02T03:04:05' date -Iseconds -d "2024-01-02 03:04:05"

echo "  ── -R (RFC 5322) ──"
assert_cmd "Tue, 02 Jan 2024 03:04:05 +0000" date -u -R -d "2024-01-02 03:04:05"

echo "  ── -r (reference file mtime) ──"
touch -d "2020-05-06 07:08:09" "$TMPDIR"/date_ref.txt
assert_cmd "2020-05-06" date +%Y-%m-%d -r "$TMPDIR"/date_ref.txt

echo "  ── invalid date string ──"
assert_cmd_pat_stderr 'invalid date' date -d "not a real date"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' date --help

echo "  ── --version ──"
assert_cmd_pat 'date \(modbox\) 1.0' date --version
