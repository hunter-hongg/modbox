SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── bc ───────────────────────────────────────"

bc() { "$MODBOX" bc "$@"; }

echo "  ── arithmetic"
assert_cmd "3" bc -q <<'EOF'
1+2
EOF
assert_cmd "7" bc -q <<'EOF'
1+2*3
EOF
assert_cmd "9" bc -q <<'EOF'
(1+2)*3
EOF
assert_cmd "1024" bc -q <<'EOF'
2^10
EOF
assert_cmd "2" bc -q <<'EOF'
5%3
EOF
assert_cmd "-2" bc -q <<'EOF'
-5+3
EOF
assert_cmd "5.0" bc -q <<'EOF'
3.5+1.5
EOF
assert_cmd "3" bc -q <<'EOF'
10/3
EOF

echo "  ── scale"
assert_cmd "3.33" bc -q <<'EOF'
scale=2; 10/3
EOF
assert_cmd "3.3333333333" bc -q <<'EOF'
scale=10; 10/3
EOF

echo "  ── big integers (BigInt core)"
assert_cmd "123456789012345678901234567891" bc -q <<'EOF'
123456789012345678901234567890 + 1
EOF
assert_cmd "999999999999999998000000000000000001" bc -q <<'EOF'
999999999999999999 * 999999999999999999
EOF
assert_cmd "1267650600228229401496703205376" bc -q <<'EOF'
2^100
EOF

echo "  ── assignment + print"
assert_cmd "5" bc -q <<'EOF'
x=5; print x
EOF
assert_cmd "5" bc -q <<'EOF'
x=5; x
EOF

echo "  ── obase / ibase conversion"
assert_cmd "FF" bc -q <<'EOF'
obase=16; 255
EOF
assert_cmd "1010" bc -q <<'EOF'
obase=2; 10
EOF
assert_cmd "255" bc -q <<'EOF'
ibase=16; FF
EOF

echo "  ── define + return"
assert_cmd "10" bc -q <<'EOF'
define f(x) { return (x*2); }
f(5)
EOF
assert_cmd "3628800" bc -q <<'EOF'
define f(x) { if (x<=1) return (1); return (x*f(x-1)); }
f(10)
EOF

echo "  ── sqrt"
assert_cmd "2" bc -q <<'EOF'
sqrt(4)
EOF
assert_cmd "1.41" bc -q <<'EOF'
scale=2; sqrt(2)
EOF

echo "  ── comparisons / equality (scale-aware)"
assert_cmd "1" bc -q <<'EOF'
2.5 > 2
EOF
assert_cmd "1" bc -q <<'EOF'
1.5 < 2
EOF
assert_cmd "1" bc -q <<'EOF'
1.0 == 1
EOF
assert_cmd "1" bc -q <<'EOF'
2 > 1
EOF

echo "  ── control flow"
assert_cmd "$(printf '1\n2\n3\n')" bc -q <<'EOF'
i=0; while (i<3) { i=i+1; print i }
EOF
assert_cmd "$(printf '0\n1\n2\n')" bc -q <<'EOF'
for (i=0; i<3; i=i+1) print i
EOF
assert_cmd "1" bc -q <<'EOF'
if (1) 1 else 2
EOF
assert_cmd "2" bc -q <<'EOF'
if (0) 1 else 2
EOF

echo "  ── comments"
assert_cmd "3" bc -q <<'EOF'
# comment
1+2
EOF
assert_cmd "3" bc -q <<'EOF'
// comment
1+2
EOF

echo "  ── errors"
assert_cmd_pat_stderr 'divide by zero' bc -q <<'EOF'
1/0
EOF
# empty input -> exit 1
if printf '' | "$MODBOX" bc -q >/dev/null 2>&1; then
    fail "empty input — expected exit 1"
else
    pass "empty input exits nonzero"
fi