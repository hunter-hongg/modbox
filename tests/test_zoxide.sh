echo ""
echo "── zoxide ────────────────────────────────────"

ZOXIDE_DB="$TMPDIR/zoxide_test_data"
mkdir -p "$ZOXIDE_DB/zoxide"
export XDG_DATA_HOME="$ZOXIDE_DB"

echo "  ── help ──"
assert_cmd_pat 'Usage:' zoxide --help
assert_cmd_pat 'Subcommands:' zoxide --help

echo "  ── init bash ──"
assert_cmd_pat '__zoxide_hook' zoxide init bash
assert_cmd_pat '^z\(\)' zoxide init bash

echo "  ── init zsh ──"
assert_cmd_pat 'precmd_functions' zoxide init zsh

echo "  ── init fish ──"
assert_cmd_pat 'function z' zoxide init fish

echo "  ── init posix ──"
assert_cmd_pat '^z\(\)' zoxide init posix

echo "  ── init nushell ──"
assert_cmd_pat 'def --env z' zoxide init nushell

echo "  ── add + list ──"
mkdir -p "$TMPDIR/zo_alpha" "$TMPDIR/zo_beta" "$TMPDIR/zo_gamma"
assert_cmd "" zoxide add "$TMPDIR/zo_alpha"
assert_cmd "" zoxide add "$TMPDIR/zo_beta"
assert_cmd "" zoxide add "$TMPDIR/zo_gamma"
assert_cmd_pat "zo_alpha" zoxide list
assert_cmd_pat "zo_beta" zoxide list
assert_cmd_pat "zo_gamma" zoxide list

echo "  ── query exact ──"
result=$("$MODBOX" zoxide query alpha 2>/dev/null)
if [[ "$result" == *"/zo_alpha" ]]; then
    pass "zoxide query alpha → matches zo_alpha"
else
    fail "zoxide query alpha → expected zo_alpha got [$result]"
fi

echo "  ── query multiple keywords ──"
result=$("$MODBOX" zoxide query zo beta 2>/dev/null)
if [[ "$result" == *"/zo_beta" ]]; then
    pass "zoxide query zo beta → matches zo_beta"
else
    fail "zoxide query zo beta → expected zo_beta got [$result]"
fi

echo "  ── query no match ──"
assert_cmd_pat_stderr "no match" zoxide query nonexistent_xyz

echo "  ── frecency: higher rank wins ──"
for i in $(seq 1 5); do
    "$MODBOX" zoxide add "$TMPDIR/zo_gamma" 2>/dev/null
done
result=$("$MODBOX" zoxide query zo 2>/dev/null)
if [[ "$result" == *"/zo_gamma" ]]; then
    pass "zoxide query zo → higher-ranked zo_gamma wins"
else
    fail "zoxide query zo → expected zo_gamma got [$result]"
fi

echo "  ── remove ──"
assert_cmd "" zoxide remove "$TMPDIR/zo_beta"
assert_cmd_not_pat "zo_beta" zoxide list

echo "  ── remove nonexistent ──"
mkdir -p "$TMPDIR/zo_notindb"
assert_cmd_pat_stderr "not found" zoxide remove "$TMPDIR/zo_notindb"

echo "  ── add nonexistent path ──"
assert_cmd_pat_stderr "cannot resolve" zoxide add "$TMPDIR/does_not_exist_xyz"

echo "  ── empty list ──"
rm -f "$ZOXIDE_DB/zoxide/db"
assert_cmd "" zoxide list

unset XDG_DATA_HOME
