SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── lspci ──────────────────────────────────────"

echo "  ── --help ──"
assert_cmd_pat 'Usage:' lspci --help

echo "  ── --version ──"
assert_cmd_pat 'lspci \(modbox\) 1\.0' lspci --version

echo "  ── unrecognized option → stderr + exit 1 ──"
assert_cmd_pat_stderr "unrecognized option" lspci --bogus

echo "  ── default output runs successfully ──"
if "$MODBOX" lspci >/dev/null 2>&1; then
    pass "lspci (default: exit 0)"
else
    fail "lspci — non-zero exit"
fi

echo "  ── default output address format (when devices present) ──"
real_out=$("$MODBOX" lspci 2>/dev/null)
if [[ -n "$real_out" ]]; then
    if printf '%s\n' "$real_out" | grep -qE '^[0-9a-f]{2}:[0-9a-f]{2}\.[0-9] '; then
        pass "lspci (default: address format)"
    else
        fail "lspci — output lacks 'BB:DD.D' address format"
    fi
    # -n must show raw hex IDs and no resolved vendor name.
    assert_cmd_pat '0x[0-9a-f]{2,}' lspci -n
else
    pass "lspci (no PCI devices on host; skipped format check)"
fi

echo "  ── --json is always valid JSON ──"
json_result=$("$MODBOX" lspci --json 2>/dev/null)
if printf '%s' "$json_result" | python3 -m json.tool >/dev/null 2>&1; then
    pass "lspci --json (valid JSON)"
else
    fail "lspci --json — expected valid JSON"
fi

echo "  ── deterministic behavior via fixture (host-independent) ──"
# Build a fake sysfs tree + pci.ids so the assertions do not depend on the
# host's actual PCI topology or hwdata files.
FIX="$TMPDIR/lspci_fixture"
mkdir -p "$FIX/sys/bus/pci/devices/0000:00:0a.0" "$FIX/hwdata"
printf '0x8086\n'   > "$FIX/sys/bus/pci/devices/0000:00:0a.0/vendor"
printf '0x1234\n'   > "$FIX/sys/bus/pci/devices/0000:00:0a.0/device"
printf '0x030000\n' > "$FIX/sys/bus/pci/devices/0000:00:0a.0/class"
printf '0x01\n'     > "$FIX/sys/bus/pci/devices/0000:00:0a.0/revision"
printf '8086  Test Vendor Corp.\n\t1234  Test Device Model X\n' > "$FIX/hwdata/pci.ids"

# Named: expect the resolved vendor/device names, class, and revision.
named_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lspci 2>/dev/null)
if [[ "$named_out" == *"00:0a.0 VGA compatible controller: Test Vendor Corp. Test Device Model X (rev 01)"* ]]; then
    pass "lspci (fixture: names resolved)"
else
    fail "lspci (fixture) — expected resolved name, got [$named_out]"
fi

# No-name: expect raw hex IDs and no resolved names.
noname_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lspci -n 2>/dev/null)
if [[ "$noname_out" == *"0x8086 0x1234"* ]] && [[ "$noname_out" != *"Test Vendor"* ]]; then
    pass "lspci -n (fixture: raw IDs, no names)"
else
    fail "lspci -n (fixture) — expected raw IDs, got [$noname_out]"
fi

# --parse: expect the selected fields, space-separated.
parse_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lspci --parse=address,vendor 2>/dev/null)
if [[ "$parse_out" == "00:0a.0 Test Vendor Corp."* ]]; then
    pass "lspci --parse (fixture: field selection)"
else
    fail "lspci --parse (fixture) — expected '00:0a.0 Test Vendor Corp.', got [$parse_out]"
fi

# Missing database: fall back to raw IDs gracefully (still exit 0).
fallback_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/missing" "$MODBOX" lspci 2>/dev/null)
if [[ "$fallback_out" == *"0x8086"* ]] && [[ -n "$fallback_out" ]]; then
    pass "lspci (fixture: graceful fallback without pci.ids)"
else
    fail "lspci (fixture) — expected raw-ID fallback, got [$fallback_out]"
fi

# JSON escaping: a name containing a literal double quote must still yield valid JSON.
FIXQ="$TMPDIR/lspci_quote"
mkdir -p "$FIXQ/sys/bus/pci/devices/0000:00:0b.0" "$FIXQ/hwdata"
printf '0x1c63\n'   > "$FIXQ/sys/bus/pci/devices/0000:00:0b.0/vendor"
printf '0x0025\n'   > "$FIXQ/sys/bus/pci/devices/0000:00:0b.0/device"
printf '0x010802\n' > "$FIXQ/sys/bus/pci/devices/0000:00:0b.0/class"
printf '1c63  Test "Quoted" Vendor\n\t0025  NVMe 2.5" Drive\n' > "$FIXQ/hwdata/pci.ids"
qjson=$(MODBOX_SYSFS="$FIXQ/sys" MODBOX_IDS_DIR="$FIXQ/hwdata" "$MODBOX" lspci --json 2>/dev/null)
if printf '%s' "$qjson" | python3 -m json.tool >/dev/null 2>&1; then
    pass "lspci --json (fixture: escapes quotes in names)"
else
    fail "lspci --json (fixture) — quote in name broke JSON: [$qjson]"
fi

# --extended: column-aligned output present.
ext_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lspci --extended 2>/dev/null)
if [[ "$ext_out" == *"00:0a.0"* && "$ext_out" == *"VGA compatible controller"* ]]; then
    pass "lspci --extended (fixture)"
else
    fail "lspci --extended (fixture) — got [$ext_out]"
fi

# --parse with an unknown field prints <unknown>.
unk_out=$(MODBOX_SYSFS="$FIX/sys" MODBOX_IDS_DIR="$FIX/hwdata" "$MODBOX" lspci --parse=address,bogus 2>/dev/null)
if [[ "$unk_out" == *"00:0a.0 <unknown>"* ]]; then
    pass "lspci --parse (fixture: unknown field)"
else
    fail "lspci --parse (fixture) — unknown field, got [$unk_out]"
fi
