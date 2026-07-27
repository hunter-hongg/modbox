SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── pathchk ─────────────────────────────────────"

echo "  ── valid existing file ──"
touch "$TMPDIR/valid_file.txt"
if "$MODBOX" pathchk "$TMPDIR/valid_file.txt" >/dev/null 2>&1; then
    pass "pathchk valid_file.txt exits 0"
else
    fail "pathchk valid_file.txt — expected exit 0"
fi

echo "  ── -p skips existence check ──"
if "$MODBOX" pathchk -p /nonexistent/portable/path >/dev/null 2>&1; then
    pass "pathchk -p /nonexistent/portable/path exits 0"
else
    fail "pathchk -p nonexistent — expected exit 0"
fi

echo "  ── nonexistent file without -p fails ──"
if "$MODBOX" pathchk /nonexistent_modbox_test_file >/dev/null 2>&1; then
    fail "pathchk nonexistent — expected non-zero exit"
else
    pass "pathchk nonexistent exits non-zero"
fi
assert_cmd_pat_stderr 'No such file' pathchk /nonexistent_modbox_test_file

echo "  ── component too long ──"
LONG_NAME=$(printf 'a%.0s' {1..300})
if "$MODBOX" pathchk -p "$LONG_NAME" >/dev/null 2>&1; then
    fail "pathchk -p 300-char name — expected non-zero exit"
else
    pass "pathchk -p 300-char name exits non-zero"
fi
assert_cmd_pat_stderr 'too long' pathchk -p "$LONG_NAME"

echo "  ── -L allows long names ──"
if "$MODBOX" pathchk -p -L "$LONG_NAME" >/dev/null 2>&1; then
    pass "pathchk -p -L 300-char name exits 0"
else
    fail "pathchk -p -L 300-char name — expected exit 0"
fi

echo "  ── -n MAX custom limit ──"
if "$MODBOX" pathchk -p -n 10 short >/dev/null 2>&1; then
    pass "pathchk -p -n 10 short exits 0"
else
    fail "pathchk -p -n 10 short — expected exit 0"
fi
if "$MODBOX" pathchk -p -n 10 waytoolongcomponent >/dev/null 2>&1; then
    fail "pathchk -p -n 10 long component — expected non-zero exit"
else
    pass "pathchk -p -n 10 long component exits non-zero"
fi

echo "  ── leading dash warning ──"
assert_cmd_pat_stderr 'Warning' pathchk -p -- -dashfile
if "$MODBOX" pathchk -p -w -- -dashfile >/dev/null 2>&1; then
    pass "pathchk -p -w -- -dashfile suppresses warning, exits 0"
else
    fail "pathchk -p -w -- -dashfile — expected exit 0"
fi

echo "  ── consecutive slashes rejected ──"
if "$MODBOX" pathchk -p "a//b" >/dev/null 2>&1; then
    fail "pathchk -p a//b — expected non-zero exit"
else
    pass "pathchk -p a//b exits non-zero"
fi

echo "  ── multiple files ──"
if "$MODBOX" pathchk -p good.txt also_good.txt >/dev/null 2>&1; then
    pass "pathchk -p multiple valid files exits 0"
else
    fail "pathchk -p multiple valid files — expected exit 0"
fi

echo "  ── path with spaces ──"
mkdir -p "$TMPDIR/my dir"
touch "$TMPDIR/my dir/file.txt"
if "$MODBOX" pathchk "$TMPDIR/my dir/file.txt" >/dev/null 2>&1; then
    pass "pathchk 'my dir/file.txt' exits 0"
else
    fail "pathchk path with spaces — expected exit 0"
fi

echo "  ── invalid option ──"
if "$MODBOX" pathchk --bogus-option file >/dev/null 2>&1; then
    fail "pathchk --bogus-option — expected non-zero exit"
else
    pass "pathchk --bogus-option exits non-zero"
fi

echo "  ── missing operand ──"
if "$MODBOX" pathchk >/dev/null 2>&1; then
    fail "pathchk with no args — expected non-zero exit"
else
    pass "pathchk with no args exits non-zero"
fi

echo "  ── --help ──"
assert_cmd_pat 'Usage: pathchk' pathchk --help

echo "  ── --version ──"
assert_cmd_pat 'pathchk \(modbox\) 1\.0' pathchk --version
