SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "-- getfacl --------------------------------------"

echo "  -- setup --"
echo "plain file" > "$TMPDIR"/gf_plain.txt
chmod 644 "$TMPDIR"/gf_plain.txt
echo "exec file" > "$TMPDIR"/gf_exec.txt
chmod 755 "$TMPDIR"/gf_exec.txt
mkdir -p "$TMPDIR"/gf_dir/sub
echo "nested" > "$TMPDIR"/gf_dir/sub/file.txt
chmod 644 "$TMPDIR"/gf_dir/sub/file.txt

# Set up ACL test files (if filesystem supports ACLs)
HAVE_ACL=0
if setfacl -m u:nobody:r "$TMPDIR"/gf_plain.txt 2>/dev/null; then
    HAVE_ACL=1
fi

echo "  -- --help --"
assert_cmd_pat 'Usage:' getfacl --help

echo "  -- --version --"
assert_cmd_pat 'GPLv3' getfacl --version

echo "  -- nonexistent file --"
assert_cmd_pat_stderr "cannot stat" getfacl "$TMPDIR"/gf_nonexistent

echo "  -- basic file output --"
assert_cmd_pat '# file:' getfacl "$TMPDIR"/gf_plain.txt
assert_cmd_pat 'user::' getfacl "$TMPDIR"/gf_plain.txt
assert_cmd_pat 'group::' getfacl "$TMPDIR"/gf_plain.txt
assert_cmd_pat 'other::' getfacl "$TMPDIR"/gf_plain.txt

echo "  -- -c: omit header --"
# With -c, should not contain '# file:' header
out=$("$MODBOX" getfacl -c "$TMPDIR"/gf_plain.txt 2>/dev/null)
if ! echo "$out" | grep -q '# file:'; then
    pass "getfacl -c omits header"
else
    fail "getfacl -c should omit header"
fi

echo "  -- -n: numeric IDs --"
assert_cmd_pat "$MY_UID" getfacl -n "$TMPDIR"/gf_plain.txt
assert_cmd_pat "$MY_GID" getfacl -n "$TMPDIR"/gf_plain.txt

echo "  -- -t: tabular format --"
# Tabular should use abbreviated tags like u::, g::, o::
assert_cmd_pat 'u::' getfacl -t "$TMPDIR"/gf_plain.txt
assert_cmd_pat 'g::' getfacl -t "$TMPDIR"/gf_plain.txt
assert_cmd_pat 'o::' getfacl -t "$TMPDIR"/gf_plain.txt

echo "  -- multiple files --"
out=$("$MODBOX" getfacl "$TMPDIR"/gf_plain.txt "$TMPDIR"/gf_exec.txt 2>/dev/null)
count=$(echo "$out" | grep -c '# file:')
if [[ "$count" -eq 2 ]]; then
    pass "getfacl multiple files → 2 file headers"
else
    fail "getfacl multiple files — expected 2 file headers, got $count"
fi

echo "  -- directory output --"
assert_cmd_pat 'user::rwx' getfacl "$TMPDIR"/gf_dir

if [[ "$HAVE_ACL" -eq 1 ]]; then
    echo "  -- ACL entries shown --"
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/gf_plain.txt

    echo "  -- -n: numeric ACL --"
    assert_cmd_pat '65534' getfacl -n "$TMPDIR"/gf_plain.txt

    echo "  -- -a: access only --"
    out=$("$MODBOX" getfacl -a "$TMPDIR"/gf_plain.txt 2>/dev/null)
    if ! echo "$out" | grep -q 'default:'; then
        pass "getfacl -a shows no default entries"
    else
        fail "getfacl -a should not show default entries"
    fi

    # Set up default ACL on directory
    if setfacl -m d:u:nobody:rx "$TMPDIR"/gf_dir 2>/dev/null; then
        echo "  -- default ACL (both access and default) --"
        out=$("$MODBOX" getfacl "$TMPDIR"/gf_dir 2>/dev/null)
        if echo "$out" | grep -q 'default:user::rwx' && echo "$out" | grep -q 'user::rwx'; then
            pass "getfacl default shows both access and default"
        else
            fail "getfacl default should show both access and default entries"
        fi

        echo "  -- -d: default only (no prefix) --"
        out=$("$MODBOX" getfacl -d "$TMPDIR"/gf_dir 2>/dev/null)
        if echo "$out" | grep -q 'user::rwx' && ! echo "$out" | grep -q 'default:user'; then
            pass "getfacl -d shows default entries without prefix"
        else
            fail "getfacl -d should show default entries without 'default:' prefix"
        fi

        echo "  -- -a -d: both with prefix --"
        out=$("$MODBOX" getfacl -a -d "$TMPDIR"/gf_dir 2>/dev/null)
        if echo "$out" | grep -q 'default:user::rwx'; then
            pass "getfacl -a -d shows default with 'default:' prefix"
        else
            fail "getfacl -a -d should show default entries with 'default:' prefix"
        fi
    fi
else
    echo "  -- (ACLs not supported, skipping ACL tests) --"
fi

echo "  -- -R: recursive traversal --"
out=$("$MODBOX" getfacl -R "$TMPDIR"/gf_dir 2>/dev/null)
# Should show parent dir and nested file
dir_count=$(echo "$out" | grep -c '# file:')
if [[ "$dir_count" -ge 2 ]]; then
    pass "getfacl -R shows multiple files ($dir_count headers)"
else
    fail "getfacl -R — expected >=2 file headers, got $dir_count"
fi

echo "  -- --preserve-root on non-root path --"
# Should not trigger for a regular path
assert_cmd_pat '# file:' getfacl -R --preserve-root "$TMPDIR"/gf_dir

echo "  -- --preserve-root on / --"
assert_cmd_pat_stderr "dangerous to operate recursively" getfacl -R --preserve-root /

echo "  -- non-recursive on directory --"
# Should still show directory's own ACL
assert_cmd_pat '# file:' getfacl "$TMPDIR"/gf_dir

echo "  -- -p: absolute names (keeps leading '/') --"
out=$("$MODBOX" getfacl -p "$TMPDIR"/gf_plain.txt 2>/dev/null)
if echo "$out" | grep -q "# file: $TMPDIR"; then
    pass "getfacl -p keeps absolute path"
else
    fail "getfacl -p should keep absolute path"
fi

echo "  -- default: strips leading '/' --"
# The stderr should contain the warning when processing an absolute path (without -p)
assert_cmd_pat_stderr "Removing leading" getfacl "$TMPDIR"/gf_exec.txt

echo "  -- -s: skip-base (plain file, no output) --"
# Plain file with no extended ACL should be skipped
out=$("$MODBOX" getfacl -s "$TMPDIR"/gf_exec.txt 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "getfacl -s skips plain file"
else
    fail "getfacl -s should skip plain file, got: $out"
fi

if [[ "$HAVE_ACL" -eq 1 ]]; then
    echo "  -- -s: skip-base (ACL file, shown) --"
    out=$("$MODBOX" getfacl -s "$TMPDIR"/gf_plain.txt 2>/dev/null)
    if echo "$out" | grep -q 'user:nobody:r--'; then
        pass "getfacl -s shows file with ACL"
    else
        fail "getfacl -s should show file with ACL"
    fi
fi

echo "  -- -H: dereference symlink in recursive mode --"
ln -sf "$TMPDIR"/gf_dir "$TMPDIR"/gf_link_dir 2>/dev/null
# Without -H, should just show the symlink itself (one header)
out=$("$MODBOX" getfacl -R "$TMPDIR"/gf_link_dir 2>/dev/null)
count=$(echo "$out" | grep -c '# file:')
if [[ "$count" -eq 1 ]]; then
    pass "getfacl -R (no -H) shows symlink only ($count header)"
else
    fail "getfacl -R (no -H) — expected 1 header, got $count"
fi
# With -H, should follow symlink into dir (multiple headers)
out=$("$MODBOX" getfacl -R -H "$TMPDIR"/gf_link_dir 2>/dev/null)
count=$(echo "$out" | grep -c '# file:')
if [[ "$count" -ge 2 ]]; then
    pass "getfacl -R -H follows symlink ($count headers)"
else
    fail "getfacl -R -H — expected >=2 headers, got $count"
fi

# Clean up ACL test files
rm -rf /tmp/acl_test.txt /tmp/acl_dir /tmp/acl_rec_test /tmp/gf_test.txt /tmp/gf_link_dir /tmp/gf_linktest_dir \
  /tmp/test_acl_text /tmp/test_acl_text.c /tmp/test_acl_text2 /tmp/test_acl_text2.c \
  /tmp/test_acl_text3 /tmp/test_acl_text3.c 2>/dev/null || true
