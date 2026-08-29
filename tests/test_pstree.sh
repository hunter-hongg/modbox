SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── pstree ───────────────────────────────────"

# Seed a deterministic, self-contained process subtree. The backgrounded
# `sleep` IS the root of the subtree we inspect; its parent is the test shell.
# We read the child's actual PID from /proc via its command line so the test
# is robust regardless of which shell layer owns it.
sleep 120 &
ROOT=$!
# Wait until /proc/<ROOT>/cmdline reflects the sleep process.
for _ in $(seq 1 20); do
    if [[ -r "/proc/$ROOT/cmdline" ]] && grep -q "sleep" "/proc/$ROOT/cmdline" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

cleanup() { kill "$ROOT" 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT

echo "  ── default: ASCII indented tree of names ──"
# The seeded sleep root appears; its parent (this test's shell) does not.
assert_cmd_pat "^sleep" pstree "$ROOT"
assert_cmd_not_pat "modbox" pstree "$ROOT"

echo "  ── PID argument: only that subtree ──"
assert_cmd_pat "^sleep" pstree "$ROOT"
assert_cmd_not_pat "modbox" pstree "$ROOT"

echo "  ── -p: PIDs shown in parentheses ──"
assert_cmd_pat "sleep\($ROOT\)" pstree -p "$ROOT"

echo "  ── -a: command-line arguments appended (no name duplication) ──"
assert_cmd_pat "sleep 120" pstree -a "$ROOT"
assert_cmd_not_pat "sleep sleep" pstree -a "$ROOT"

echo "  ── -u: uid/username annotation ──"
# The seeded chain runs as the test user, so a user annotation is emitted.
assert_cmd_pat "$(id -un)" pstree -u "$ROOT"

echo "  ── -s: focused ancestor chain above the root ──"
# -s <root> prints the chain from init down to <root> and its subtree only;
# the rest of the forest (e.g. the modbox process) must NOT appear.
assert_cmd_pat "sleep" pstree -s "$ROOT"
assert_cmd_not_pat "modbox" pstree -s "$ROOT"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' pstree --help

echo "  ── -V / --version ──"
assert_cmd_pat 'pstree \(modbox\) 1\.0' pstree -V
assert_cmd_pat 'pstree \(modbox\) 1\.0' pstree --version

echo "  ── error: invalid option ──"
assert_cmd_pat_stderr 'invalid option' pstree --bogus

echo "  ── error: -s without a PID ──"
assert_cmd_pat_stderr 'requires a PID' pstree -s

echo ""
echo "── pstree tests complete ──"
