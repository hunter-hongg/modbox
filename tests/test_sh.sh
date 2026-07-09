# Test suite for sh command

# ── Basic -c mode ──────────────────────────────────────────────────────────
assert_cmd "hello world" sh -c "echo hello world"
assert_cmd "42"          sh -c "echo 42"

# ── Exit code (checked via stderr/next command) ────────────────────────────
assert_cmd "0" sh -c "echo \$?"

# ── Variable expansion ─────────────────────────────────────────────────────
assert_cmd "foo" sh -c "X=foo; echo \$X"
assert_cmd "0"   sh -c "echo \$?"
assert_cmd "bar" sh -c "X=bar; echo \${X}"

# ── Single quotes (literal) ────────────────────────────────────────────────
assert_cmd '$foo' sh -c "echo '\$foo'"
assert_cmd "hello" sh -c "echo 'hello'"

# ── Double quotes (expansion preserved) ────────────────────────────────────
assert_cmd "hello" sh -c "X=hello; echo \"\$X\""

# ── Command not found ──────────────────────────────────────────────────────
assert_cmd_pat_stderr "not found" sh -c "nonexistent_cmd_xyz_12345"

# ── Comments ───────────────────────────────────────────────────────────────
assert_cmd "hello" sh -c "echo hello # this is a comment"

# ── I/O Redirection (within single sh invocation) ──────────────────────────
# stdout to file, then read it back
assert_cmd "hello" sh -c "echo hello > \"$TMPDIR/redir_test.txt\"; cat \"$TMPDIR/redir_test.txt\""
# stdin from file
assert_cmd "hello" sh -c "cat < \"$TMPDIR/redir_test.txt\""
# stderr redirection
# echo writes to stdout; 2> redirects stderr (which has no output from echo)
assert_cmd "errmsg" sh -c "echo errmsg 2> \"$TMPDIR/err_test.txt\""
# Actually test stderr redirect with a command that writes to stderr
assert_cmd "" sh -c "echo errmsg >&2 2> \"$TMPDIR/err_stderr_test.txt\"; cat \"$TMPDIR/err_stderr_test.txt\""
# append
assert_cmd "" sh -c "echo line1 > \"$TMPDIR/append_test.txt\"; echo line2 >> \"$TMPDIR/append_test.txt\""
assert_cmd "line1" sh -c "head -1 \"$TMPDIR/append_test.txt\""
assert_cmd "line2" sh -c "tail -1 \"$TMPDIR/append_test.txt\""

# ── Pipes ──────────────────────────────────────────────────────────────────
assert_cmd "hello"   sh -c "echo hello | cat"
assert_cmd "HELLO"   sh -c "echo hello | tr a-z A-Z"
assert_cmd "2"       sh -c "echo hello world | wc -w | tr -d ' '"

# ── cd / pwd ───────────────────────────────────────────────────────────────
assert_cmd "$TMPDIR" sh -c "cd \"$TMPDIR\" && pwd"

# ── set -e ─────────────────────────────────────────────────────────────────
assert_cmd "before" sh -c "echo before; set -e; false; echo should_not_reach"

# ── set -x ─────────────────────────────────────────────────────────────────
assert_cmd "hello" sh -c "set -x; echo hello" 2>/dev/null

# ── ${VAR:-default} ────────────────────────────────────────────────────────
assert_cmd "default" sh -c "echo \${UNDEFINED_VAR:-default}"

# ── Builtins: export ───────────────────────────────────────────────────────
assert_cmd "exported" sh -c "X=exported; export X; echo \$X"

# ── Builtins: unset ────────────────────────────────────────────────────────
assert_cmd "" sh -c "X=foo; unset X; echo \$X"

# ── Builtins: type ─────────────────────────────────────────────────────────
assert_cmd_pat "builtin" sh -c "type echo"
assert_cmd_pat "builtin" sh -c "type cd"
assert_cmd_pat "builtin" sh -c "type exit"

# ── Builtins: . (source) ───────────────────────────────────────────────────
echo "VAR_FROM_SOURCE=sourced" > "$TMPDIR/source_test.sh"
assert_cmd "sourced" sh -c ". \"$TMPDIR/source_test.sh\"; echo \$VAR_FROM_SOURCE"

# ── if/then/else/fi ────────────────────────────────────────────────────────
assert_cmd "then-branch" sh -c "if true; then echo then-branch; else echo else-branch; fi"
assert_cmd "else-branch" sh -c "if false; then echo then-branch; else echo else-branch; fi"

# ── if/elif/else/fi ────────────────────────────────────────────────────────
assert_cmd "elif-branch" sh -c "if false; then echo first; elif true; then echo elif-branch; else echo else-branch; fi"

# ── for/do/done ────────────────────────────────────────────────────────────
assert_cmd_pat "a" sh -c "for i in a b c; do echo \$i; done"
assert_cmd_pat "b" sh -c "for i in a b c; do echo \$i; done"
assert_cmd_pat "c" sh -c "for i in a b c; do echo \$i; done"

# ── while/do/done (verify while works without hanging) ──────────────────────
assert_cmd "" sh -c "while false; do echo should_not_reach; done"

# ── case/esac ──────────────────────────────────────────────────────────────
assert_cmd "match" sh -c "case foo in bar) echo no;; foo) echo match;; esac"
assert_cmd "default" sh -c "case foo in bar) echo no;; *) echo default;; esac"

# ── && / || ────────────────────────────────────────────────────────────────
assert_cmd "both" sh -c "true && echo both"
assert_cmd "or"   sh -c "false || echo or"
assert_cmd ""     sh -c "false && echo should_not_reach"

# ── Script file execution ──────────────────────────────────────────────────
cat > "$TMPDIR/test_script.sh" << 'SCRIPT'
echo hello from script
X=42
echo $X
SCRIPT
assert_cmd_pat "hello from script" sh "$TMPDIR/test_script.sh"
assert_cmd_pat "42"               sh "$TMPDIR/test_script.sh"

# ── echo builtin ───────────────────────────────────────────────────────────
assert_cmd "a b c" sh -c "echo a b c"

# ── cd with no args (to HOME) ──────────────────────────────────────────────
assert_cmd "$HOME" sh -c "cd; pwd"

# ── Variable expansion in double quotes ────────────────────────────────────
assert_cmd "hello world" sh -c "MSG=hello; echo \"\$MSG world\""

# ── Quoting edge cases ─────────────────────────────────────────────────────
assert_cmd "foo" sh -c "echo \"foo\""
assert_cmd "foo  bar" sh -c "echo foo\ \ bar"
