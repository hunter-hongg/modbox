SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "-- setfacl --------------------------------------"

echo "  -- setup --"
echo "test content" > "$TMPDIR"/sf_plain.txt
chmod 644 "$TMPDIR"/sf_plain.txt
echo "exec content" > "$TMPDIR"/sf_exec.txt
chmod 755 "$TMPDIR"/sf_exec.txt
mkdir -p "$TMPDIR"/sf_dir

HAVE_ACL=0
if setfacl -m u:root:r "$TMPDIR"/sf_plain.txt 2>/dev/null; then
    HAVE_ACL=1
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
fi

echo "  -- --help --"
assert_cmd_pat 'Usage:' setfacl --help

echo "  -- --version --"
assert_cmd_pat 'GPLv3' setfacl --version

echo "  -- missing file --"
"$MODBOX" setfacl 2>/dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "setfacl with no args exits non-zero ($rc)"
else
    fail "setfacl with no args should exit non-zero"
fi

echo "  -- nonexistent file --"
"$MODBOX" setfacl -m u:root:r /nonexistent_file_xyz 2>/dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "setfacl on nonexistent file exits non-zero ($rc)"
else
    fail "setfacl on nonexistent file should exit non-zero"
fi

if [[ "$HAVE_ACL" -eq 1 ]]; then
    # ── -m: modify ACL entries ──────────────────────────────────────────

    echo "  -- -m: add user ACL entry --"
    "$MODBOX" setfacl -m u:nobody:r "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/sf_plain.txt

    echo "  -- -m: modify existing user ACL entry --"
    "$MODBOX" setfacl -m u:nobody:rw "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'user:nobody:rw-' getfacl "$TMPDIR"/sf_plain.txt

    echo "  -- -m: add group ACL entry --"
    "$MODBOX" setfacl -m g:root:r "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'group:root:r--' getfacl "$TMPDIR"/sf_plain.txt

    echo "  -- -m: add mask entry (with -n) --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    "$MODBOX" setfacl -n -m m::rwx "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'mask::rwx' getfacl "$TMPDIR"/sf_plain.txt

    echo "  -- -m: add default user ACL on directory --"
    "$MODBOX" setfacl -m d:u:nobody:rx "$TMPDIR"/sf_dir 2>&1 || true
    assert_cmd_pat 'default:user:nobody:r-x' getfacl "$TMPDIR"/sf_dir

    # ── -n / --no-mask ──────────────────────────────────────────────────

    echo "  -- -n: no mask recalculation --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    "$MODBOX" setfacl -m u:nobody:r "$TMPDIR"/sf_plain.txt 2>/dev/null
    # mask should have been recalculated; now add another entry with -n
    "$MODBOX" setfacl -n -m u:daemon:rx "$TMPDIR"/sf_plain.txt 2>/dev/null
    # Should still work without error
    assert_cmd_pat 'user:daemon:r-x' getfacl "$TMPDIR"/sf_plain.txt

    # ── --test: dry-run ─────────────────────────────────────────────────

    echo "  -- --test: dry-run does not modify --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    # Capture state before
    before=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    "$MODBOX" setfacl --test -m u:nobody:rwx "$TMPDIR"/sf_plain.txt 2>/dev/null
    after=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    if [[ "$before" == "$after" ]]; then
        pass "setfacl --test does not modify file"
    else
        fail "setfacl --test should not modify file"
    fi

    # ── -x: remove specific entries ─────────────────────────────────────

    echo "  -- -x: remove user ACL entry --"
    "$MODBOX" setfacl -m u:nobody:rw "$TMPDIR"/sf_plain.txt 2>/dev/null
    "$MODBOX" setfacl -x u:nobody "$TMPDIR"/sf_plain.txt 2>/dev/null
    out=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    if ! echo "$out" | grep -q 'user:nobody'; then
        pass "setfacl -x removes user entry"
    else
        fail "setfacl -x should remove user entry"
    fi

    echo "  -- -x: remove group ACL entry --"
    "$MODBOX" setfacl -m g:root:rw "$TMPDIR"/sf_plain.txt 2>/dev/null
    "$MODBOX" setfacl -x g:root "$TMPDIR"/sf_plain.txt 2>/dev/null
    out=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    if ! echo "$out" | grep -q 'group:root'; then
        pass "setfacl -x removes group entry"
    else
        fail "setfacl -x should remove group entry"
    fi

    echo "  -- -x: remove default ACL entry --"
    "$MODBOX" setfacl -m d:u:nobody:rx "$TMPDIR"/sf_dir 2>/dev/null
    "$MODBOX" setfacl -x d:u:nobody "$TMPDIR"/sf_dir 2>/dev/null
    out=$("$MODBOX" getfacl -d "$TMPDIR"/sf_dir 2>/dev/null)
    if ! echo "$out" | grep -q 'user:nobody'; then
        pass "setfacl -x removes default entry"
    else
        fail "setfacl -x should remove default entry"
    fi

    # ── -b: remove all extended ACLs ────────────────────────────────────

    echo "  -- -b: remove all extended entries --"
    "$MODBOX" setfacl -m u:nobody:rw "$TMPDIR"/sf_plain.txt 2>/dev/null
    "$MODBOX" setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null
    out=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    if ! echo "$out" | grep -q 'user:nobody'; then
        pass "setfacl -b removes extended entries"
    else
        fail "setfacl -b should remove all extended entries"
    fi
    # Core entries should still be present
    assert_cmd_pat 'user::' getfacl "$TMPDIR"/sf_plain.txt
    assert_cmd_pat 'group::' getfacl "$TMPDIR"/sf_plain.txt
    assert_cmd_pat 'other::' getfacl "$TMPDIR"/sf_plain.txt

    # ── -k: remove default ACL ──────────────────────────────────────────

    echo "  -- -k: remove default ACL from directory --"
    "$MODBOX" setfacl -m d:u:nobody:rx "$TMPDIR"/sf_dir 2>/dev/null
    "$MODBOX" setfacl -k "$TMPDIR"/sf_dir 2>/dev/null
    out=$("$MODBOX" getfacl "$TMPDIR"/sf_dir 2>/dev/null)
    if ! echo "$out" | grep -q 'default:'; then
        pass "setfacl -k removes default ACL"
    else
        fail "setfacl -k should remove all default entries"
    fi

    # ── --set: replace ACL entirely ─────────────────────────────────────

    echo "  -- --set: replace ACL --"
    "$MODBOX" setfacl --set "u::rwx,g::r-x,o::---" "$TMPDIR"/sf_plain.txt 2>/dev/null
    out=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    if echo "$out" | grep -q 'user::rwx' && echo "$out" | grep -q 'other::---'; then
        pass "setfacl --set replaces ACL"
    else
        fail "setfacl --set should replace ACL entirely"
    fi

    echo "  -- --set: reject invalid ACL --"
    "$MODBOX" setfacl --set "invalid_acl_text" "$TMPDIR"/sf_plain.txt 2>/dev/null
    rc=$?
    if [[ $rc -ne 0 ]]; then
        pass "setfacl --set rejects invalid ACL (exit $rc)"
    else
        fail "setfacl --set should reject invalid ACL"
    fi

    echo "  -- --set-file: set ACL from file --"
    echo "u::rw-,g::r--,o::r--" > "$TMPDIR"/sf_acl_spec
    "$MODBOX" setfacl --set-file "$TMPDIR"/sf_acl_spec "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'user::rw-' getfacl "$TMPDIR"/sf_plain.txt
    assert_cmd_pat 'other::r--' getfacl "$TMPDIR"/sf_plain.txt

    # ── -M: modify from file ────────────────────────────────────────────

    echo "  -- -M: modify from file --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    printf 'u:nobody:rx\ng:root:r\n' > "$TMPDIR"/sf_mod_entries
    "$MODBOX" setfacl -M "$TMPDIR"/sf_mod_entries "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'user:nobody:r-x' getfacl "$TMPDIR"/sf_plain.txt
    assert_cmd_pat 'group:root:r--' getfacl "$TMPDIR"/sf_plain.txt

    echo "  -- -M: read from stdin with '-' --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    echo "u:nobody:w" | "$MODBOX" setfacl -M - "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'user:nobody:-w-' getfacl "$TMPDIR"/sf_plain.txt

    # ── -X: remove from file ────────────────────────────────────────────

    echo "  -- -X: remove from file --"
    "$MODBOX" setfacl -m u:nobody:rw "$TMPDIR"/sf_plain.txt 2>/dev/null
    "$MODBOX" setfacl -m g:root:r "$TMPDIR"/sf_plain.txt 2>/dev/null
    echo "u:nobody" > "$TMPDIR"/sf_rm_entries
    "$MODBOX" setfacl -X "$TMPDIR"/sf_rm_entries "$TMPDIR"/sf_plain.txt 2>/dev/null
    out=$("$MODBOX" getfacl "$TMPDIR"/sf_plain.txt 2>/dev/null)
    if ! echo "$out" | grep -q 'user:nobody' && echo "$out" | grep -q 'group:root'; then
        pass "setfacl -X removes specified entries only"
    else
        fail "setfacl -X should remove only specified entries"
    fi

    # ── --restore ───────────────────────────────────────────────────────

    echo "  -- --restore: restore from getfacl backup --"
    echo "restore content" > "$TMPDIR"/sf_restore.txt
    chmod 644 "$TMPDIR"/sf_restore.txt
    "$MODBOX" setfacl -m u:nobody:rwx "$TMPDIR"/sf_restore.txt 2>/dev/null
    "$MODBOX" getfacl -p "$TMPDIR"/sf_restore.txt > "$TMPDIR"/sf_backup 2>/dev/null || true
    # Modify the file so restore has something to do
    "$MODBOX" setfacl -b "$TMPDIR"/sf_restore.txt 2>/dev/null
    "$MODBOX" setfacl --restore "$TMPDIR"/sf_backup 2>&1 || true
    assert_cmd_pat 'user:nobody:rwx' getfacl "$TMPDIR"/sf_restore.txt

    # ── -R: recursive traversal ─────────────────────────────────────────

    echo "  -- -R: recursive modify --"
    mkdir -p "$TMPDIR"/sf_rec/sub1/sub2
    echo "a" > "$TMPDIR"/sf_rec/file_a.txt
    echo "b" > "$TMPDIR"/sf_rec/sub1/file_b.txt
    echo "c" > "$TMPDIR"/sf_rec/sub1/sub2/file_c.txt
    "$MODBOX" setfacl -R -m u:nobody:r "$TMPDIR"/sf_rec 2>/dev/null
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/sf_rec/file_a.txt
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/sf_rec/sub1/file_b.txt
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/sf_rec/sub1/sub2/file_c.txt

    echo "  -- -R: exits 0 when some files fail --"
    # If a file in the tree has an error, the traversal should continue
    # and the overall exit code should still reflect errors
    # (we test that -R on a nonexistent path fails)
    "$MODBOX" setfacl -R -m u:nobody:r /nonexistent_path_xyz 2>/dev/null
    rc=$?
    if [[ $rc -ne 0 ]]; then
        pass "setfacl -R on nonexistent path exits non-zero ($rc)"
    else
        fail "setfacl -R on nonexistent path should exit non-zero"
    fi

    echo "  -- --preserve-root on / --"
    assert_cmd_pat_stderr "dangerous" setfacl -R --preserve-root -m u:nobody:r /

    echo "  -- -L: follow symlinks in traversal --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    ln -sf "$TMPDIR"/sf_plain.txt "$TMPDIR"/sf_link 2>/dev/null
    "$MODBOX" setfacl -R -L -m u:nobody:r "$TMPDIR"/sf_link 2>/dev/null
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/sf_plain.txt
    rm -f "$TMPDIR"/sf_link

    echo "  -- -H: dereference CLI symlinks --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    ln -sf "$TMPDIR"/sf_plain.txt "$TMPDIR"/sf_link2 2>/dev/null
    "$MODBOX" setfacl -R -H -m u:nobody:r "$TMPDIR"/sf_link2 2>/dev/null
    assert_cmd_pat 'user:nobody:r--' getfacl "$TMPDIR"/sf_plain.txt
    rm -f "$TMPDIR"/sf_link2

    # ── Operation ordering ──────────────────────────────────────────────

    echo "  -- combined -m and -x --"
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    "$MODBOX" setfacl -m u:nobody:rwx -m g:root:r -x u:daemon "$TMPDIR"/sf_plain.txt 2>/dev/null
    assert_cmd_pat 'user:nobody:rwx' getfacl "$TMPDIR"/sf_plain.txt
    assert_cmd_pat 'group:root:r--' getfacl "$TMPDIR"/sf_plain.txt

    # Clean up ACLs
    setfacl -b "$TMPDIR"/sf_plain.txt 2>/dev/null || true
    setfacl -b "$TMPDIR"/sf_exec.txt 2>/dev/null || true
    setfacl -b "$TMPDIR"/sf_dir 2>/dev/null || true
    setfacl -k "$TMPDIR"/sf_dir 2>/dev/null || true
else
    echo "  -- (ACLs not supported, skipping ACL tests) --"
fi

# Cleanup
rm -rf /tmp/acl_test.txt /tmp/acl_dir /tmp/acl_rec_test /tmp/test_acl_text \
  /tmp/test_acl_text.c /tmp/test_acl_text2 /tmp/test_acl_text2.c \
  /tmp/test_acl_text3 /tmp/test_acl_text3.c 2>/dev/null || true
