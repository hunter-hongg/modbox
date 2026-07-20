#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lf ──────────────────────────────────────"

echo "  ── lf --help includes --tui ──"
assert_cmd_pat '\-\-tui' "$MODBOX" lf --help 2>/dev/null

echo "  ── lf non-TTY falls back to plain ls ──"
assert_cmd_pat 'regular\.txt' "$MODBOX" lf "$TMPDIR"/ls_dir 2>/dev/null
