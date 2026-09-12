SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── which ─────────────────────────────────────"

# Controlled PATH tree for deterministic cases.
WDIR="$TMPDIR/which"
mkdir -p "$WDIR/one" "$WDIR/two"

make_exe() {
    printf '#!/bin/sh\nexit 0\n' > "$1"
    chmod +x "$1"
}

make_exe "$WDIR/one/tool"
make_exe "$WDIR/two/tool"
make_exe "$WDIR/one/solo"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' which --help

echo "  ── --version shows version ──"
assert_cmd_pat '^which \(modbox\) ' which --version
# -V is the short form
assert_cmd_pat '^which \(modbox\) ' which -V

echo "  ── first match wins by default ──"
out=$(PATH="$WDIR/one:$WDIR/two" "$MODBOX" which tool 2>/dev/null)
[[ "$out" == "$WDIR/one/tool" ]] && pass "default stops at first match" \
    || fail "default first match — got [$out]"

echo "  ── -a prints all matches in PATH order ──"
out=$(PATH="$WDIR/one:$WDIR/two" "$MODBOX" which -a tool 2>/dev/null | tr '\n' ' ')
[[ "$out" == "$WDIR/one/tool $WDIR/two/tool " ]] && pass "-a prints both matches" \
    || fail "-a all matches — got [$out]"

echo "  ── --all long form ──"
out=$(PATH="$WDIR/one:$WDIR/two" "$MODBOX" which --all tool 2>/dev/null | wc -l)
[[ "$out" -eq 2 ]] && pass "--all prints both matches" \
    || fail "--all — expected 2 lines, got [$out]"

echo "  ── found command exits 0 ──"
if PATH="$WDIR/one:$WDIR/two" "$MODBOX" which tool >/dev/null 2>&1; then
    pass "which tool → exit 0"
else
    fail "which tool → expected exit 0"
fi

echo "  ── not-found prints stderr and exits 2 ──"
out=$(PATH="$WDIR/one:$WDIR/two" "$MODBOX" which nosuchcmd 2>&1 1>/dev/null)
rc=$?
if [[ $rc -eq 2 && "$out" == *"no nosuchcmd in ("* ]]; then
    pass "not-found → stderr message + exit 2"
else
    fail "not-found — rc=$rc out=[$out]"
fi

echo "  ── partial resolution exits 1 and still prints the found one ──"
out=$(PATH="$WDIR/one:$WDIR/two" "$MODBOX" which tool nosuchcmd 2>/dev/null)
rc=$?
if [[ $rc -eq 1 && "$out" == "$WDIR/one/tool" ]]; then
    pass "partial → prints match, exit 1"
else
    fail "partial — rc=$rc out=[$out]"
fi

echo "  ── empty exit when all names resolve ──"
if PATH="$WDIR/one:$WDIR/two" "$MODBOX" which tool tool >/dev/null 2>&1; then
    pass "all resolved → exit 0"
else
    fail "all resolved → expected exit 0"
fi

echo "  ── empty PATH segments are skipped ──"
out=$(PATH="$WDIR/one::$WDIR/two" "$MODBOX" which -a tool 2>/dev/null | wc -l)
[[ "$out" -eq 2 ]] && pass "empty segment skipped" \
    || fail "empty segment — expected 2 matches, got [$out]"

echo "  ── name containing a slash is used directly ──"
printf '#!/bin/sh\n' > "$WDIR/direct"; chmod +x "$WDIR/direct"
out=$("$MODBOX" which "$WDIR/direct" 2>/dev/null)
[[ "$out" == "$WDIR/direct" ]] && pass "slash name used directly" \
    || fail "slash name — got [$out]"

echo "  ── slash name that is not executable fails ──"
printf 'not exe\n' > "$WDIR/noexec"; chmod -x "$WDIR/noexec"
if "$MODBOX" which "$WDIR/noexec" >/dev/null 2>&1; then
    fail "non-executable slash name → expected non-zero"
else
    pass "non-executable slash name → exit non-zero"
fi

echo "  ── --skip-dot ignores dot-prefixed PATH entries ──"
mkdir -p "$WDIR/cwd2/.hidden"
make_exe "$WDIR/cwd2/.hidden/tool"
make_exe "$WDIR/one/tool"
# A PATH entry whose string begins with '.' (including '.' itself) is skipped;
# an absolute path into a dot-directory is NOT skipped (matches GNU which).
out=$(cd "$WDIR/cwd2" && PATH=".hidden:$WDIR/one" "$MODBOX" which --skip-dot tool 2>/dev/null)
[[ "$out" == "$WDIR/one/tool" ]] && pass "--skip-dot skips dot-prefixed dir" \
    || fail "--skip-dot — got [$out]"
# Without --skip-dot, the dot-prefixed entry wins.
out=$(cd "$WDIR/cwd2" && PATH=".hidden:$WDIR/one" "$MODBOX" which tool 2>/dev/null)
[[ "$out" == "$WDIR/cwd2/.hidden/tool" ]] && pass "dot-prefixed dir used without --skip-dot" \
    || fail "dot-prefixed dir — got [$out]"
# An absolute path into a dot-directory is not affected by --skip-dot.
out=$(PATH="$WDIR/cwd2/.hidden:$WDIR/one" "$MODBOX" which --skip-dot tool 2>/dev/null)
[[ "$out" == "$WDIR/cwd2/.hidden/tool" ]] && pass "--skip-dot keeps absolute dot-dir" \
    || fail "--skip-dot absolute — got [$out]"

echo "  ── --skip-tilde ignores tilde-prefixed PATH entries ──"
out=$(PATH="~nomatch:$WDIR/one" "$MODBOX" which --skip-tilde tool 2>/dev/null)
[[ "$out" == "$WDIR/one/tool" ]] && pass "--skip-tilde skips tilde dir" \
    || fail "--skip-tilde — got [$out]"

echo "  ── '.' PATH entry resolves to cwd ──"
mkdir -p "$WDIR/cwdtest"
make_exe "$WDIR/cwdtest/tool"
out=$(cd "$WDIR/cwdtest" && PATH=".:$WDIR/one" "$MODBOX" which tool 2>/dev/null)
[[ "$out" == "$WDIR/cwdtest/tool" ]] && pass ". entry expanded to cwd" \
    || fail ". entry — got [$out]"

echo "  ── --show-dot keeps '.' in output ──"
out=$(cd "$WDIR/cwdtest" && PATH=".:$WDIR/one" "$MODBOX" which --show-dot tool 2>/dev/null)
[[ "$out" == "./tool" ]] && pass "--show-dot keeps dot" \
    || fail "--show-dot — got [$out]"

echo "  ── --show-tilde renders HOME prefix ──"
if [[ "$(id -u)" -ne 0 ]]; then
    out=$(HOME="$WDIR/one" PATH="$WDIR/one:$WDIR/two" "$MODBOX" which --show-tilde tool 2>/dev/null)
    [[ "$out" == "~/tool" ]] && pass "--show-tilde renders ~" \
        || fail "--show-tilde — got [$out]"
else
    pass "--show-tilde skipped (running as root)"
fi

echo "  ── -- terminator stops option parsing ──"
make_exe "$WDIR/one/-a"
out=$(PATH="$WDIR/one" "$MODBOX" which -- -a 2>/dev/null)
[[ "$out" == "$WDIR/one/-a" ]] && pass "-- treats -a as a name" \
    || fail "-- terminator — got [$out]"

echo "  ── unknown option → stderr + exit 2 ──"
out=$(PATH="$WDIR/one" "$MODBOX" which -Z tool 2>&1 1>/dev/null)
rc=$?
if [[ $rc -eq 2 && "$out" == *"invalid option -- 'Z'"* ]]; then
    pass "unknown option → stderr + exit 2"
else
    fail "unknown option — rc=$rc out=[$out]"
fi

echo "  ── no arguments prints usage and exits 0 ──"
out=$(PATH="$WDIR/one" "$MODBOX" which 2>/dev/null)
rc=$?
if [[ $rc -eq 0 && "$out" == Usage:* ]]; then
    pass "no args → usage + exit 0"
else
    fail "no args — rc=$rc"
fi
