SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── whereis ─────────────────────────────────────"

echo "  ── --help shows usage ──"
assert_cmd_pat 'Usage:' whereis --help

echo "  ── --version shows version ──"
assert_cmd_pat '^whereis \(modbox\) ' whereis --version
assert_cmd_pat '^whereis \(modbox\) ' whereis -V

echo "  ── real-system lookup: whereis ls contains /usr/bin/ls ──"
assert_cmd_pat '^ls: .*/usr/bin/ls' whereis ls

echo "  ── -b restricts to binaries (no man paths) ──"
out=$("$MODBOX" whereis -b ls 2>/dev/null)
if [[ "$out" == "ls: /usr/bin/ls" ]]; then
    pass "-b prints only the binary"
else
    fail "-b — got [$out]"
fi

echo "  ── no names prints nothing and exits 0 ──"
out=$("$MODBOX" whereis 2>/dev/null)
rc=$?
if [[ -z "$out" && $rc -eq 0 ]]; then
    pass "no names → empty output + exit 0"
else
    fail "no names — rc=$rc out=[$out]"
fi

echo "  ── not-found name prints 'name:' and exits 0 ──"
out=$("$MODBOX" whereis nonexistcmd12345 2>/dev/null)
rc=$?
if [[ "$out" == "nonexistcmd12345:" && $rc -eq 0 ]]; then
    pass "not-found → 'name:' + exit 0"
else
    fail "not-found — rc=$rc out=[$out]"
fi

# ── Controlled tree ────────────────────────────────────────────────────
WDIR="$TMPDIR/whereis"
mkdir -p "$WDIR/bin" "$WDIR/man" "$WDIR/src" "$WDIR/man2"

# A complete entry: binary + man (gzip'd) + source.
printf '#!/bin/sh\n' > "$WDIR/bin/full"; chmod +x "$WDIR/bin/full"
printf 'int main(void){return 0;}\n' > "$WDIR/src/full.c"
gzip -c <(echo '.TH FULL 1') > "$WDIR/man/full.1.gz"

# A binary with no source.
printf '#!/bin/sh\n' > "$WDIR/bin/nosrc"; chmod +x "$WDIR/bin/nosrc"
gzip -c <(echo 'x') > "$WDIR/man/nosrc.1.gz"

# A name not present at all.
# A name with two man pages (duplicate class).

echo "  ── controlled tree: complete entry, fixed order ──"
out=$("$MODBOX" whereis -b -B "$WDIR/bin" -f -m -M "$WDIR/man" -f -s -S "$WDIR/src" -f full 2>/dev/null)
expected="full: $WDIR/bin/full $WDIR/man/full.1.gz $WDIR/src/full.c"
if [[ "$out" == "$expected" ]]; then
    pass "controlled tree → b/m/s in fixed order"
else
    fail "controlled tree — expected [$expected] got [$out]"
fi

echo "  ── -B only replaces bin roots; man/src keep defaults ──"
out=$("$MODBOX" whereis -b -B "$WDIR/bin" -f full 2>/dev/null)
if [[ "$out" == "full: $WDIR/bin/full" ]]; then
    pass "-B replaces only bin roots"
else
    fail "-B only — got [$out]"
fi

echo "  ── -m restrict to manuals ──"
out=$("$MODBOX" whereis -m -M "$WDIR/man" -f full 2>/dev/null)
if [[ "$out" == "full: $WDIR/man/full.1.gz" ]]; then
    pass "-m prints only the manual"
else
    fail "-m — got [$out]"
fi

echo "  ── -s restrict to sources ──"
out=$("$MODBOX" whereis -s -S "$WDIR/src" -f full 2>/dev/null)
if [[ "$out" == "full: $WDIR/src/full.c" ]]; then
    pass "-s prints only the source"
else
    fail "-s — got [$out]"
fi

echo "  ── -u prints unusual (missing class) ──"
out=$("$MODBOX" whereis -u -b -B "$WDIR/bin" -f -m -M "$WDIR/man" -f -s -S "$WDIR/src" -f nosrc 2>/dev/null)
if [[ "$out" == "nosrc: $WDIR/bin/nosrc $WDIR/man/nosrc.1.gz" ]]; then
    pass "-u prints name with a missing class"
else
    fail "-u missing class — got [$out]"
fi

echo "  ── -u suppresses complete entries ──"
out=$("$MODBOX" whereis -u -b -B "$WDIR/bin" -f -m -M "$WDIR/man" -f -s -S "$WDIR/src" -f full 2>/dev/null)
if [[ -z "$out" ]]; then
    pass "-u suppresses complete entry"
else
    fail "-u complete — expected empty, got [$out]"
fi

echo "  ── -u treats not-found as unusual ──"
out=$("$MODBOX" whereis -u -b -B "$WDIR/bin" -f -m -M "$WDIR/man" -f -s -S "$WDIR/src" -f nosuchname 2>/dev/null)
if [[ "$out" == "nosuchname:" ]]; then
    pass "-u not-found → 'name:'"
else
    fail "-u not-found — got [$out]"
fi

echo "  ── -u treats duplicated class as unusual ──"
gzip -c <(echo 'x') > "$WDIR/man2/full.2.gz"
gzip -c <(echo 'y') > "$WDIR/man2/full.3.gz"
out=$("$MODBOX" whereis -u -m -M "$WDIR/man2" -f full 2>/dev/null)
if [[ "$out" == *"full: "* && "$out" == *"full.2.gz"* && "$out" == *"full.3.gz"* ]]; then
    pass "-u duplicated class → unusual"
else
    fail "-u duplicated — got [$out]"
fi

echo "  ── -l prints lookup paths and exits 0 ──"
out=$("$MODBOX" whereis -l 2>/dev/null)
rc=$?
if [[ $rc -eq 0 && "$out" == *"bin: "* ]]; then
    pass "-l prints lookup paths + exit 0"
else
    fail "-l — rc=$rc out=[${out:0:60}]"
fi

echo "  ── -l with -B reflects the override ──"
assert_cmd_pat "bin: $WDIR/bin" whereis -l -B "$WDIR/bin" -f

echo "  ── unknown option → stderr + exit 2 ──"
out=$("$MODBOX" whereis -Z ls 2>&1 1>/dev/null)
rc=$?
if [[ $rc -eq 2 && "$out" == *"unrecognized option"* ]]; then
    pass "unknown option → stderr + exit 2"
else
    fail "unknown option — rc=$rc out=[$out]"
fi

echo "  ── -- terminator: following args are names ──"
out=$("$MODBOX" whereis -- -b 2>/dev/null)
if [[ "$out" == "-b:" ]]; then
    pass "-- treats -b as a name"
else
    fail "-- terminator — got [$out]"
fi

echo "  ── name with a slash is labelled by its basename ──"
out=$("$MODBOX" whereis -b /usr/bin/ls 2>/dev/null)
if [[ "$out" == "ls: /usr/bin/ls" ]]; then
    pass "slashed name → basename label"
else
    fail "basename label — got [$out]"
fi
