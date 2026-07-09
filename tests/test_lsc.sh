SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lsc ──────────────────────────────────────"

mkdir -p "$TMPDIR"/lsc_dir
touch "$TMPDIR"/lsc_dir/regular.txt
touch "$TMPDIR"/lsc_dir/exec.sh
chmod +x "$TMPDIR"/lsc_dir/exec.sh
mkdir "$TMPDIR"/lsc_dir/subdir
touch "$TMPDIR"/lsc_dir/.hidden

echo "  ── basic lsc (colorful + icons) ──"
assert_cmd_pat 'regular\.txt' lsc "$TMPDIR"/lsc_dir

echo "  ── lsc shows icons ──"
assert_cmd_pat $'\xef\x80\x96' lsc "$TMPDIR"/lsc_dir 2>/dev/null  #  file icon

echo "  ── lsc shows ANSI color codes ──"
assert_cmd_pat $'\x1b\[' lsc "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -l : long format with colors ──"
assert_cmd_pat 'regular\.txt' lsc -l "$TMPDIR"/lsc_dir 2>/dev/null
assert_cmd_pat '^[-d]' lsc -l "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -a : show all (including hidden) ──"
assert_cmd_pat '\.hidden' lsc -a "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -A : almost all — no . .. ──"
assert_cmd_pat '\.hidden' lsc -A "$TMPDIR"/lsc_dir 2>/dev/null
assert_cmd_not_pat '(^| )\.\.?  *' lsc -A "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -F : classify with indicators ──"
assert_cmd_pat 'subdir/' lsc -F "$TMPDIR"/lsc_dir 2>/dev/null
assert_cmd_pat 'exec\.sh\*' lsc -F "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc -r : reverse sort ──"
assert_cmd_pat 'regular' lsc -r "$TMPDIR"/lsc_dir 2>/dev/null

echo "  ── lsc --help shows usage ──"
assert_cmd_pat 'Usage:' lsc --help 2>/dev/null

echo "  ── lsc non-existent directory error ──"
assert_cmd_pat_stderr 'No such file' lsc "$TMPDIR"/lsc_nonexistent
