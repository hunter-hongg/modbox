#!/usr/bin/env bash
#
# framework.sh — Shared test helpers for modbox
# Sourced by run_tests.sh and individual test_*.sh files.
#

# Do NOT set errexit — test failures use non-zero returns via grep/[[ ]] etc.
set -o nounset

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODBOX=$(readlink -f "$SCRIPT_DIR/../target/modbox")
PASS_COUNT=0
FAIL_COUNT=0
TMPDIR=$(mktemp -d /tmp/modbox_test.XXXXXX)
trap 'rm -rf "$TMPDIR"' EXIT
MY_UID=$(id -u)
MY_GID=$(id -g)

# ── Test framework ──────────────────────────────────────────────────────────

pass()  { echo "  PASS  $*"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail()  { echo "  FAIL  $*"; FAIL_COUNT=$((FAIL_COUNT + 1)); }

# assert_cmd EXPECTED_OUTPUT args...
# Runs: modbox <args...> and compares stdout to EXPECTED_OUTPUT
assert_cmd() {
    local expected="$1"; shift
    local actual
    actual=$("$MODBOX" "$@" 2>/dev/null || true)
    if [[ "$actual" == "$expected" ]]; then
        pass "$*"
    else
        fail "$* — expected [$(echo "$expected" | head -c 80)] got [$(echo "$actual" | head -c 80)]"
    fi
}

# assert_cmd_pat PATTERN args...
# Runs modbox <args> and checks stdout contains PATTERN (extended regex)
assert_cmd_pat() {
    local pattern="$1"; shift
    if "$MODBOX" "$@" 2>/dev/null | grep -qE "$pattern"; then
        pass "$* → matches /$pattern/"
    else
        fail "$* — expected pattern /$pattern/ not found in output"
    fi
}

# assert_cmd_not_pat PATTERN args...
assert_cmd_not_pat() {
    local pattern="$1"; shift
    if "$MODBOX" "$@" 2>/dev/null | grep -qE "$pattern"; then
        fail "$* — unexpected pattern /$pattern/ found"
    else
        pass "$* — correctly lacks /$pattern/"
    fi
}

# assert_cmd_pat_stderr PATTERN args...
assert_cmd_pat_stderr() {
    local pattern="$1"; shift
    if "$MODBOX" "$@" 2>&1 1>/dev/null | grep -qE "$pattern"; then
        pass "$* → stderr matches /$pattern/"
    else
        fail "$* — expected stderr pattern /$pattern/ not found"
    fi
}
