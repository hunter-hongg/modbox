SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── printenv ─────────────────────────────────────"

export MODBOX_TEST_VAR=foo

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' printenv --help

echo "  ── prints known env var ──"
assert_cmd "MODBOX_TEST_VAR=foo" printenv MODBOX_TEST_VAR

echo "  ── prints PATH ──"
assert_cmd "PATH=$PATH" printenv PATH

echo "  ── missing var exits non-zero ──"
if "$MODBOX" printenv MODBOX_NO_SUCH_VAR_XYZ </dev/null >/dev/null 2>&1; then
    fail "missing var → expected non-zero"
else
    pass "missing var → exit non-zero"
fi

echo "  ── no args prints all env vars ──"
assert_cmd_pat 'PATH=' printenv

echo "  ── -0 NUL separator ──"
out=$(MODBOX_TEST_VAR=foo "$MODBOX" printenv -0 MODBOX_TEST_VAR | od -An -c)
if printf '%s' "$out" | grep -q '\0'; then
    pass "-0 emits NUL"
else
    fail "-0 NUL separator got [$out]"
fi
