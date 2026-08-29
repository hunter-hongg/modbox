#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "  ── lf --help includes --tui ──"
assert_cmd_pat '\-\-tui' lf --help 2>/dev/null

mkdir -p "$TMPDIR"/ls_dir
touch    "$TMPDIR"/ls_dir/regular.txt
touch    "$TMPDIR"/ls_dir/.hidden
mkdir    "$TMPDIR"/ls_dir/subdir

echo "  ── lf non-TTY falls back to plain ls ──"
assert_cmd_pat 'regular\.txt' lf "$TMPDIR"/ls_dir 2>/dev/null

echo "  ── lf --color=never falls back without ANSI ──"
lf_output=$("$MODBOX" lf --color=never "$TMPDIR"/ls_dir 2>/dev/null)
if printf '%s' "$lf_output" | grep -qE 'regular\.txt'; then
    pass "lf --color=never non-TTY → plain output with regular.txt"
else
    fail "lf --color=never non-TTY → missing regular.txt in output"
fi
if printf '%s' "$lf_output" | grep -q $'\033'; then
    fail "lf --color=never non-TTY → unexpected ANSI codes"
else
    pass "lf --color=never non-TTY → no ANSI codes"
fi
