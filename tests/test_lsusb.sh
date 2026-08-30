SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lsusb ──────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' lsusb --help

echo "  ── --version ──"
assert_cmd_pat 'lsusb \(modbox\) 1\.0' lsusb --version

echo "  ── unrecognized option → stderr + exit 1 ──"
assert_cmd_pat_stderr "unrecognized option" lsusb --bogus

echo "  ── default output runs successfully ──"
if "$MODBOX" lsusb >/dev/null 2>&1; then
    pass "lsusb (default: exit 0)"
else
    fail "lsusb — non-zero exit"
fi

echo "  ── default output ID format (when devices present) ──"
real_out=$("$MODBOX" lsusb 2>/dev/null)
if [[ -n "$real_out" ]]; then
    if printf '%s\n' "$real_out" | grep -qE 'ID [0-9a-f]{4}:[0-9a-f]{4}'; then
        pass "lsusb (default: 'ID vid:pid' format)"
    else
        fail "lsusb — output lacks 'ID vid:pid' format"
    fi
    # -n must show raw IDs and no resolved vendor name.
    assert_cmd_pat '[0-9a-f]{4}:[0-9a-f]{4}' lsusb -n
else
    pass "lsusb (no USB devices on host; skipped format check)"
fi

echo "  ── --json is always valid JSON ──"
json_result=$("$MODBOX" lsusb --json 2>/dev/null)
if printf '%s' "$json_result" | python3 -m json.tool >/dev/null 2>&1; then
    pass "lsusb --json (valid JSON)"
else
    fail "lsusb --json — expected valid JSON"
fi

echo "  ── deterministic behavior via fixture (host-independent) ──"
# Build a fake USB sysfs tree + usb.ids so the assertions do not depend on the
# host's actual USB topology or hwdata files.
FIX="$TMPDIR/lsusb_fixture"
mkdir -p "$FIX/sys/bus/usb/devices/1-1" "$FIX/hwdata"
printf '17ef\n' > "$FIX/sys/bus/usb/devices/1-1/idVendor"
printf '623c\n' > "$FIX/sys/bus/usb/devices/1-1/idProduct"
printf '00\n'   > "$FIX/sys/bus/usb/devices/1-1/bDeviceClass"
printf '1\n'    > "$FIX/sys/bus/usb/devices/1-1/busnum"
printf '2\n'    > "$FIX/sys/bus/usb/devices/1-1/devnum"
printf '17ef  Test USB Vendor\n\t623c  Test USB Product\n' > "$FIX/hwdata/usb.ids"

# Named: expect the resolved vendor name in the standard "Bus X Device Y" form.
named_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lsusb 2>/dev/null)
if [[ "$named_out" == *"Bus 001 Device 002: ID 17ef:623c Test USB Vendor"* ]]; then
    pass "lsusb (fixture: vendor name resolved)"
else
    fail "lsusb (fixture) — expected resolved name, got [$named_out]"
fi

# No-name: expect raw IDs and no resolved names.
noname_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lsusb -n 2>/dev/null)
if [[ "$noname_out" == *"Bus 001 Device 002: ID 17ef:623c"* ]] && [[ "$noname_out" != *"Test USB Vendor"* ]]; then
    pass "lsusb -n (fixture: raw IDs, no names)"
else
    fail "lsusb -n (fixture) — expected raw IDs, got [$noname_out]"
fi

# Missing database: fall back to raw IDs gracefully (still exit 0).
fallback_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/missing" "$MODBOX" lsusb 2>/dev/null)
if [[ "$fallback_out" == *"17ef:623c"* ]] && [[ -n "$fallback_out" ]]; then
    pass "lsusb (fixture: graceful fallback without usb.ids)"
else
    fail "lsusb (fixture) — expected raw-ID fallback, got [$fallback_out]"
fi

# JSON escaping: a name with a literal double quote must still yield valid JSON.
FIXQ="$TMPDIR/lsusb_quote"
mkdir -p "$FIXQ/sys/bus/usb/devices/2-2" "$FIXQ/hwdata"
printf '2424\n' > "$FIXQ/sys/bus/usb/devices/2-2/idVendor"
printf '4001\n' > "$FIXQ/sys/bus/usb/devices/2-2/idProduct"
printf '09\n'   > "$FIXQ/sys/bus/usb/devices/2-2/bDeviceClass"
printf '2\n'    > "$FIXQ/sys/bus/usb/devices/2-2/busnum"
printf '3\n'    > "$FIXQ/sys/bus/usb/devices/2-2/devnum"
printf '2424  HP "Wide" Labs\n\t4001  19" Monitor Hub\n' > "$FIXQ/hwdata/usb.ids"
qjson=$(MODBOX_SYSFS="$FIXQ/sys" MODBOX_IDS_DIR="$FIXQ/hwdata" "$MODBOX" lsusb --json 2>/dev/null)
if printf '%s' "$qjson" | python3 -m json.tool >/dev/null 2>&1; then
    pass "lsusb --json (fixture: escapes quotes in names)"
else
    fail "lsusb --json (fixture) — quote in name broke JSON: [$qjson]"
fi
