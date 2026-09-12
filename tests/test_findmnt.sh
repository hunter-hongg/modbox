SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── findmnt ────────────────────────────────────"

# A deterministic mount table so tree/filter assertions do not depend on the
# host's live layout. Format mirrors /proc/self/mountinfo:
#   ID PARENT MAJ:MIN ROOT TARGET OPTS [TAGS] - FSTYPE SOURCE SUPER-OPTS
FIXTURE="$TMPDIR/findmnt_mountinfo"
cat > "$FIXTURE" <<'EOF'
32 2 0:29 /@ / rw,relatime shared:1 - btrfs /dev/mapper/root rw,compress=zstd:3
26 32 0:24 / /proc rw,nosuid shared:5 - proc proc rw
27 32 0:25 / /sys rw shared:6 - sysfs sys rw
40 27 0:30 / /sys/kernel/debug rw - debugfs debugfs rw
50 32 0:31 / /mnt/data rw - ext4 /dev/sda1 rw
EOF

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' findmnt --help
assert_cmd_pat 'TARGET' findmnt --help

echo "  ── --version / -V ──"
assert_cmd_pat 'findmnt \(modbox\) 1\.0' findmnt --version
assert_cmd_pat 'findmnt \(modbox\) 1\.0' findmnt -V

echo "  ── help findmnt is discoverable ──"
assert_cmd_pat 'findmnt' help findmnt

echo "  ── unrecognized option exits non-zero with argtable3 wording ──"
assert_cmd_pat_stderr "unrecognized option '--nope'" findmnt --nope
"$MODBOX" findmnt --nope >/dev/null 2>&1; rc=$?
if [[ $rc -ne 0 ]]; then pass "findmnt --nope → non-zero exit ($rc)"; else fail "findmnt --nope → expected non-zero, got 0"; fi

echo "  ── unknown column exits 2 ──"
assert_cmd_pat_stderr "unknown column 'BOGUS'" findmnt -o BOGUS
"$MODBOX" findmnt -o BOGUS >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "findmnt -o BOGUS → exit 2"; else fail "findmnt -o BOGUS → expected 2, got $rc"; fi

echo "  ── --fstab is rejected with exit 2 ──"
assert_cmd_pat_stderr 'not supported' findmnt --fstab
"$MODBOX" findmnt --fstab >/dev/null 2>&1; rc=$?
if [[ $rc -eq 2 ]]; then pass "findmnt --fstab → exit 2"; else fail "findmnt --fstab → expected 2, got $rc"; fi

echo "  ── missing option argument errors ──"
assert_cmd_pat_stderr 'Try .* --help' findmnt -o
assert_cmd_pat_stderr "unexpected argument" findmnt / /proc

echo "  ── live table: / is always present ──"
assert_cmd_pat '^/' findmnt
MODBOX_CANON=$("$MODBOX" findmnt -c | awk '{print $1}' | grep -cxE '/')
if [[ "$MODBOX_CANON" -ge 1 ]]; then pass "findmnt -c lists the root mount /"; else fail "findmnt -c did not list /"; fi

echo "  ── query semantics ──"
target_out=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt /proc)
printf '%s' "$target_out" | grep -qE '^/proc ' && pass "findmnt /proc matches the /proc row" || fail "findmnt /proc did not match"
printf '%s' "$target_out" | grep -qv '/sys' && pass "findmnt /proc excludes /sys" || fail "findmnt /proc leaked /sys"

echo "  ── -t filters filesystem types ──"
MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -n -o TARGET -t proc | grep -qE '/proc' && pass "findmnt -t proc selects /proc" || fail "findmnt -t proc missing /proc"
"$MODBOX" findmnt -t nonexistentfs >/dev/null 2>&1; rc=$?
if [[ $rc -eq 1 ]]; then pass "findmnt -t nonexistentfs → exit 1"; else fail "findmnt -t nonexistentfs → expected 1, got $rc"; fi
out=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -t nonexistentfs 2>/dev/null)
if [[ -z "$out" ]]; then pass "findmnt -t nonexistentfs → no stdout"; else fail "findmnt -t nonexistentfs → unexpected stdout [$out]"; fi

echo "  ── -v inverts the type filter ──"
out=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -n -o FSTYPE -v -t proc)
printf '%s' "$out" | grep -qE 'proc' && fail "findmnt -v -t proc still showed proc" || pass "findmnt -v -t proc excludes proc"

echo "  ── -o column selection shapes the header ──"
hdr=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -o TARGET,SOURCE | head -1)
printf '%s' "$hdr" | grep -qE 'TARGET' && printf '%s' "$hdr" | grep -qE 'SOURCE' && pass "findmnt -o TARGET,SOURCE header has both columns" || fail "findmnt -o TARGET,SOURCE header wrong [$hdr]"
printf '%s' "$hdr" | grep -qE 'FSTYPE' && fail "header unexpectedly contains FSTYPE" || pass "header omits unselected FSTYPE"

echo "  ── -n suppresses the header ──"
out=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -n -o TARGET,SOURCE)
printf '%s' "$out" | grep -qE 'TARGET' && fail "findmnt -n still printed header" || pass "findmnt -n suppresses header"

echo "  ── -r raw pairs ──"
assert_cmd_pat 'target="/"' findmnt -r -o TARGET

echo "  ── -P pairs uses uppercase keys ──"
assert_cmd_pat 'TARGET="/"' findmnt -P -o TARGET

echo "  ── -c canonical emits three space-separated fields ──"
MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -c | grep -qE '^/[^ ]* [^ ]+ [^ ]+$' && pass "findmnt -c → three space-separated fields" || fail "findmnt -c output shape wrong"

echo "  ── -l flat list ──"
out=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -l -n -o TARGET)
printf '%s' "$out" | grep -qE '└─|├─|│' && fail "findmnt -l still drew tree glyphs" || pass "findmnt -l is a flat list"
printf '%s' "$out" | grep -qE '^/sys/kernel/debug$' && pass "findmnt -l lists nested target flat" || fail "findmnt -l dropped the nested target"

echo "  ── tree nests children under their parent ──"
tree=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -n -o TARGET)
printf '%s' "$tree" | grep -qE '└─ /proc|├─ /proc' && pass "findmnt tree draws a branch glyph for /proc" || fail "findmnt tree missing branch glyph for /proc"
printf '%s' "$tree" | grep -qE '│  └─ /sys/kernel/debug|  └─ /sys/kernel/debug' && pass "findmnt tree indents /sys/kernel/debug deeper" || fail "findmnt tree did not nest /sys/kernel/debug"

echo "  ── -J emits JSON that round-trips through jq ──"
json=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -J)
n=$(printf '%s' "$json" | "$MODBOX" jq 'length' 2>/dev/null)
if [[ "$n" == "5" ]]; then pass "findmnt -J → JSON array of 5 mounts"; else fail "findmnt -J → expected 5 elements, got [$n]"; fi
first=$(printf '%s' "$json" | "$MODBOX" jq '.[0].target' 2>/dev/null)
if [[ "$first" == '"/"' ]]; then pass "findmnt -J → first object target is /"; else fail "findmnt -J → first target [$first]"; fi
ft=$(printf '%s' "$json" | "$MODBOX" jq '.[0].fstype' 2>/dev/null)
if [[ "$ft" == '"btrfs"' ]]; then pass "findmnt -J → first object fstype is btrfs"; else fail "findmnt -J → first fstype [$ft]"; fi

echo "  ── -R prints submounts of a target ──"
out=$(MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -R -n -o TARGET /sys)
printf '%s' "$out" | grep -qE '/sys/kernel/debug' && pass "findmnt -R /sys lists the submount" || fail "findmnt -R /sys missing submount"
printf '%s' "$out" | grep -qxE '/sys' && fail "findmnt -R /sys still listed /sys itself" || pass "findmnt -R /sys omits the mount itself"

echo "  ── -S filters by source device ──"
MODBOX_MOUNTINFO="$FIXTURE" "$MODBOX" findmnt -n -o TARGET -S /dev/sda1 | grep -qE '/mnt/data' && pass "findmnt -S selects by source" || fail "findmnt -S did not match"

echo "  ── fallback to /proc/mounts when mountinfo is unreadable ──"
out=$(MODBOX_MOUNTINFO=/nonexistent/findmnt_mountinfo "$MODBOX" findmnt -c)
printf '%s' "$out" | awk '{print $1}' | grep -qxE '/' && pass "findmnt falls back to /proc/mounts (found /)" || fail "findmnt fallback did not print /"
printf '%s' "$out" | grep -qxE '/proc proc proc' && pass "findmnt fallback parsed /proc correctly" || fail "findmnt fallback misparsed /proc"
