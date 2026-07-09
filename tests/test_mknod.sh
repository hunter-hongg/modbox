echo ""
echo "── mknod ────────────────────────────────────"

# Determine if we can create device nodes (requires root)
if [ "$MY_UID" = "0" ]; then
    CAN_DEVICE=1
else
    CAN_DEVICE=0
fi

echo "  ── create FIFO ──"
assert_cmd "" mknod "$TMPDIR"/mknod_fifo p
test -p "$TMPDIR"/mknod_fifo && pass "mknod_fifo is a FIFO" || fail "mknod_fifo not created"

echo "  ── already exists ──"
assert_cmd_pat_stderr "File exists" mknod "$TMPDIR"/mknod_fifo p

echo "  ── FIFO with mode ──"
assert_cmd "" mknod -m 0640 "$TMPDIR"/mknod_fifo_mode p
mode=$(stat -c '%a' "$TMPDIR"/mknod_fifo_mode 2>/dev/null)
if [ "$mode" = "640" ]; then
    pass "mknod -m 0640 → mode 640"
else
    fail "mknod -m 0640 → expected mode 640 got [$mode]"
fi

if [ "$CAN_DEVICE" = "1" ]; then
    echo "  ── character device ──"
    assert_cmd "" mknod "$TMPDIR"/mknod_chr c 1 3
    if [ -c "$TMPDIR"/mknod_chr ]; then
        pass "mknod chr c 1 3 is a character device"
    else
        fail "mknod chr c 1 3 — expected character device"
    fi

    echo "  ── character device (u) ──"
    assert_cmd "" mknod "$TMPDIR"/mknod_chr_u u 1 3
    if [ -c "$TMPDIR"/mknod_chr_u ]; then
        pass "mknod chr u 1 3 is a character device"
    else
        fail "mknod chr u 1 3 — expected character device"
    fi

    echo "  ── block device ──"
    assert_cmd "" mknod "$TMPDIR"/mknod_blk b 8 0
    if [ -b "$TMPDIR"/mknod_blk ]; then
        pass "mknod blk b 8 0 is a block device"
    else
        fail "mknod blk b 8 0 — expected block device"
    fi
else
    echo "  ── character device (non-root: expect EPERM) ──"
    assert_cmd_pat_stderr "Operation not permitted" mknod "$TMPDIR"/mknod_chr c 1 3

    echo "  ── character device u (non-root: expect EPERM) ──"
    assert_cmd_pat_stderr "Operation not permitted" mknod "$TMPDIR"/mknod_chr_u u 1 3

    echo "  ── block device (non-root: expect EPERM) ──"
    assert_cmd_pat_stderr "Operation not permitted" mknod "$TMPDIR"/mknod_blk b 8 0
fi

echo "  ── missing device number ──"
assert_cmd_pat_stderr "missing" mknod "$TMPDIR"/mknod_nodev c

echo "  ── extra operand for FIFO ──"
assert_cmd_pat_stderr "extra operand" mknod "$TMPDIR"/mknod_extra p 1 2

echo "  ── invalid type ──"
assert_cmd_pat_stderr "invalid device type" mknod "$TMPDIR"/mknod_bad x

echo "  ── help ──"
assert_cmd_pat 'Usage:' mknod --help
