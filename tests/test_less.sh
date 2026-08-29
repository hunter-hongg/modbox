#!/usr/bin/env bash
#
# test_less.sh — Tests for the modbox less command (GNU-aligned pager).
#
# The harness runs with stdin=/dev/null and stdout non-TTY, so the
# interactive raw-mode path is NOT exercised here; instead we verify the
# pager module's non-TTY passthrough contract plus the CLI surface
# (help, version, usage errors, file errors). Interactive key handling
# (j/k/space/search) is covered by manual/pty harness — see Further Notes
# in the spec.

source "$(dirname "${BASH_SOURCE[0]}")/framework.sh"

echo "=== less command tests ==="

# 1. --help prints Usage
assert_cmd_pat "Usage:" less --help

# 2. --version prints modbox
assert_cmd_pat "modbox" less --version

# 3. Unknown option -> usage error, exit code 2 (repo convention)
"$MODBOX" less --bogus >/dev/null 2>&1
rc=$?
if [[ "$rc" -eq 2 ]]; then
  pass "less --bogus exits 2"
else
  fail "less --bogus exits 2 (got $rc)"
fi

# 4. Missing file -> error on stderr, exit code 1
"$MODBOX" less "$TMPDIR/no-such-file-xyz" >/dev/null 2>&1
rc=$?
if [[ "$rc" -eq 1 ]]; then
  pass "less <missing file> exits 1"
else
  fail "less <missing file> exits 1 (got $rc)"
fi
assert_cmd_pat_stderr "No such file" less "$TMPDIR/no-such-file-xyz"

# Prepare fixture files
printf 'hello\nworld\n' > "$TMPDIR/less_a.txt"
printf 'foo\nbar\nbaz\n' > "$TMPDIR/less_b.txt"

# 5. Single file non-TTY passthrough behaves like cat
EXPECTED=$(cat "$TMPDIR/less_a.txt")
assert_cmd "$EXPECTED" less "$TMPDIR/less_a.txt"

# 6. stdin non-TTY passthrough (script stdin is /dev/null, so feed inline)
OUT=$(printf 'a\nb\nc\n' | "$MODBOX" less 2>/dev/null)
if [[ "$OUT" == "$(printf 'a\nb\nc\n')" ]]; then
  pass "less (stdin) passthrough"
else
  fail "less (stdin) passthrough (got [$OUT])"
fi

# 7. -N renders a 7-wide right-justified line-number gutter + single space (GNU default)
EXPECTED=$(printf '      1 hello\n      2 world\n')
assert_cmd "$EXPECTED" less -N "$TMPDIR/less_a.txt"
assert_cmd_pat "hello" less "$TMPDIR/less_a.txt" "$TMPDIR/less_b.txt"
assert_cmd_pat "baz"   less "$TMPDIR/less_a.txt" "$TMPDIR/less_b.txt"

echo ""
echo "=== less command tests complete ==="
