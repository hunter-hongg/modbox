echo ""
echo "── chown ──────────────────────────────────────────"

echo "  ── setup ──"
echo "testchown" > "$TMPDIR"/chown_a.txt
touch "$TMPDIR"/chown_b.txt
mkdir -p "$TMPDIR"/chown_dir/sub
echo "nested" > "$TMPDIR"/chown_dir/sub/file.txt
ln -s chown_a.txt "$TMPDIR"/chown_link.txt
chmod 644 "$TMPDIR"/chown_a.txt

echo "  ── --help ──"
assert_cmd_pat 'Usage:' chown --help

echo "  ── missing operand ──"
assert_cmd_pat_stderr 'missing operand' chown

echo "  ── invalid owner ──"
assert_cmd_pat_stderr 'invalid owner' chown nosuchuser99xx "$TMPDIR"/chown_a.txt

echo "  ── change owner by name ──"
"$MODBOX" chown "$(id -un)" "$TMPDIR"/chown_a.txt 2>/dev/null || true
owner=$(stat -c '%u' "$TMPDIR"/chown_a.txt)
if [[ "$owner" == "$MY_UID" ]]; then
    pass "chown <user> → owner is $owner"
else
    fail "chown <user> — expected uid $MY_UID, got $owner"
fi

echo "  ── change owner by numeric uid ──"
"$MODBOX" chown "$MY_UID" "$TMPDIR"/chown_b.txt 2>/dev/null || true
owner=$(stat -c '%u' "$TMPDIR"/chown_b.txt)
if [[ "$owner" == "$MY_UID" ]]; then
    pass "chown <uid> → owner is $owner"
else
    fail "chown <uid> — expected uid $MY_UID, got $owner"
fi

echo "  ── change owner:group ──"
touch "$TMPDIR"/chown_og.txt
"$MODBOX" chown "$MY_UID:$MY_GID" "$TMPDIR"/chown_og.txt 2>/dev/null || true
owner=$(stat -c '%u' "$TMPDIR"/chown_og.txt)
group=$(stat -c '%g' "$TMPDIR"/chown_og.txt)
if [[ "$owner" == "$MY_UID" && "$group" == "$MY_GID" ]]; then
    pass "chown <uid>:<gid> → owner=$owner group=$group"
else
    fail "chown <uid>:<gid> — expected $MY_UID:$MY_GID, got $owner:$group"
fi

echo "  ── change only group (:group) ──"
touch "$TMPDIR"/chown_justgrp.txt
"$MODBOX" chown ":$MY_GID" "$TMPDIR"/chown_justgrp.txt 2>/dev/null || true
group=$(stat -c '%g' "$TMPDIR"/chown_justgrp.txt)
if [[ "$group" == "$MY_GID" ]]; then
    pass "chown :<gid> → group=$group"
else
    fail "chown :<gid> — expected $MY_GID, got $group"
fi

echo "  ── -v: verbose output ──"
echo "verbose" > "$TMPDIR"/chown_verbose.txt
assert_cmd_pat "changed ownership" chown -v "$MY_UID" "$TMPDIR"/chown_verbose.txt

echo "  ── -c: no output when unchanged ──"
echo "changeonly" > "$TMPDIR"/chown_cc.txt
out=$("$MODBOX" chown -c "$MY_UID" "$TMPDIR"/chown_cc.txt 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "chown -c — no output when unchanged"
else
    fail "chown -c — unexpected output: [$out]"
fi

echo "  ── -f: silent (suppress error) ──"
out=$("$MODBOX" chown -f "$MY_UID" "$TMPDIR"/chown_nonexistent 2>&1 || true)
if [[ -z "$out" ]]; then
    pass "chown -f → no error output"
else
    fail "chown -f — unexpected output: [$out]"
fi

echo "  ── --reference: copy ownership from ref file ──"
echo "refsrc" > "$TMPDIR"/chown_refsrc.txt
echo "reftgt" > "$TMPDIR"/chown_reftgt.txt
chown "$MY_UID:$MY_GID" "$TMPDIR"/chown_refsrc.txt 2>/dev/null || true
"$MODBOX" chown --reference="$TMPDIR"/chown_refsrc.txt "$TMPDIR"/chown_reftgt.txt 2>/dev/null || true
owner=$(stat -c '%u' "$TMPDIR"/chown_reftgt.txt)
if [[ "$owner" == "$MY_UID" ]]; then
    pass "chown --reference → owner is $owner"
else
    fail "chown --reference — expected uid $MY_UID, got $owner"
fi

echo "  ── -R: recursive ──"
"$MODBOX" chown -R "$MY_UID:$MY_GID" "$TMPDIR"/chown_dir 2>/dev/null || true
dir_owner=$(stat -c '%u' "$TMPDIR"/chown_dir)
file_owner=$(stat -c '%u' "$TMPDIR"/chown_dir/sub/file.txt)
if [[ "$dir_owner" == "$MY_UID" && "$file_owner" == "$MY_UID" ]]; then
    pass "chown -R → dir=$dir_owner file=$file_owner"
else
    fail "chown -R — expected both $MY_UID, got dir=$dir_owner file=$file_owner"
fi

echo "  ── -Rv: recursive verbose ──"
mkdir -p "$TMPDIR"/chown_rvdir/sub
touch "$TMPDIR"/chown_rvdir/sub/f.txt
assert_cmd_pat "changed ownership" chown -Rv "$MY_UID" "$TMPDIR"/chown_rvdir

echo "  ── -h: no-dereference (affect symlink, not target) ──"
link_owner_before=$(stat -c '%u' "$TMPDIR"/chown_link.txt)
# Stat the symlink itself with -h
link_self_owner_before=$(stat -c '%u' "$TMPDIR"/chown_link.txt 2>/dev/null || echo "skip")
"$MODBOX" chown -h 0 "$TMPDIR"/chown_link.txt 2>/dev/null || true
target_owner=$(stat -c '%u' "$TMPDIR"/chown_a.txt)
if [[ "$target_owner" == "$MY_UID" ]]; then
    pass "chown -h → target owner unchanged ($target_owner)"
else
    fail "chown -h — target owner changed to $target_owner (expected $MY_UID)"
fi
# Restore symlink to our uid (may fail if we can't lchown as non-root)
"$MODBOX" chown -h "$MY_UID" "$TMPDIR"/chown_link.txt 2>/dev/null || true
