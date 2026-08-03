#!/usr/bin/env bash
# Test suite for curl command
# Sources the shared test framework.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

echo ""
echo "── curl ───────────────────────────────────────"

# ── Argument validation (no network required) ──

echo "  ── --help ──"
assert_cmd_pat 'Usage: curl' curl --help
"$MODBOX" curl --help 2>&1 | head -1 | grep -q . && pass "curl --help exits 0" || fail "curl --help failed"

echo "  ── --version ──"
assert_cmd_pat 'curl \(modbox\) 1\.0' curl --version

echo "  ── no URL ──"
assert_cmd_pat_stderr 'no URL specified' curl

echo "  ── unknown option ──"
assert_cmd_pat_stderr 'invalid option' curl --foo

echo "  ── invalid URL ──"
assert_cmd_pat_stderr 'failed to parse URL' curl '://bad'

echo "  ── -o and -O conflict ──"
assert_cmd_pat_stderr 'conflict' curl -o /tmp/out.txt -O http://127.0.0.1:39059/ 2>/dev/null || true

echo "  ── -I with -d conflict ──"
assert_cmd_pat_stderr 'not supported' curl -I -d "hello" http://127.0.0.1:39059/ 2>/dev/null || true

# ── Local HTTP server tests ──

echo ""
echo "  ── Local HTTP server tests ──"

if ! command -v python3 >/dev/null 2>&1; then
    echo "  SKIP — python3 not available for test HTTP server"
    return 0
fi

# Write test server script to a temp file
TEST_SERVER_SCRIPT="$TMPDIR/test_server.py"
cat > "$TEST_SERVER_SCRIPT" <<'PYEOF'
import http.server
import socketserver
import sys

class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/redirect':
            self.send_response(302)
            self.send_header('Location', '/final')
            self.end_headers()
        elif self.path == '/final':
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'final destination')
        elif self.path == '/404':
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'not found')
        elif self.path.startswith('/echo'):
            self.send_response(200)
            self.end_headers()
            self.wfile.write(f'path={self.path}\nheaders:'.encode())
            for k, v in self.headers.items():
                self.wfile.write(f'\n  {k}: {v}'.encode())
        else:
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'hello world')
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        ct = self.headers.get('Content-Type', '')
        self.send_response(200)
        self.end_headers()
        self.wfile.write(f'POST body: {body.decode()} CT={ct}'.encode())
    def do_PUT(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(f'PUT body: {body.decode()}'.encode())
    def do_DELETE(self):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'DELETE ok')
    def log_message(self, *a): pass

with socketserver.TCPServer(('127.0.0.1', 0), H) as s:
    print(s.server_address[1])
    sys.stdout.flush()
    s.serve_forever()
PYEOF

python3 "$TEST_SERVER_SCRIPT" > "$TMPDIR/port.txt" &
TEST_SERVER_PID=$!
sleep 0.3

# Read port from the file
PORT=$(cat "$TMPDIR/port.txt" 2>/dev/null)

# Verify server started
if ! kill -0 "$TEST_SERVER_PID" 2>/dev/null; then
    echo "  SKIP — could not start test HTTP server"
    return 0
fi

echo "  ── GET request ──"
assert_cmd 'hello world' curl -s "http://127.0.0.1:$PORT/"

echo "  ── POST request ──"
result=$("$MODBOX" curl -s -d "data=hello" "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -q 'POST body: data=hello'; then
    pass "curl -d sends POST body correctly"
else
    fail "curl -d POST body — expected body with 'data=hello', got [$(echo "$result" | head -c 80)]"
fi

echo "  ── POST Content-Type default ──"
result=$("$MODBOX" curl -s -d "data=hello" "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -qi 'CT=application/x-www-form-urlencoded'; then
    pass "curl -d sets default Content-Type"
else
    fail "curl -d Content-Type — expected application/x-www-form-urlencoded, got [$(echo "$result" | head -c 80)]"
fi

echo "  ── Custom Content-Type override ──"
result=$("$MODBOX" curl -s -H "Content-Type: application/json" -d '{"key":"val"}' "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -q 'CT=application/json'; then
    pass "curl -H Content-Type overrides default"
else
    fail "curl -H Content-Type — expected application/json, got [$(echo "$result" | head -c 80)]"
fi

echo "  ── Custom header (-H) ──"
result=$("$MODBOX" curl -s -H "X-Test: value123" "http://127.0.0.1:$PORT/echo" 2>/dev/null)
if echo "$result" | grep -q 'X-Test: value123'; then
    pass "curl -H sends custom header"
else
    fail "curl -H custom header — expected X-Test: value123"
fi

echo "  ── User-Agent (-A) ──"
result=$("$MODBOX" curl -s -A "MyAgent/1.0" "http://127.0.0.1:$PORT/echo" 2>/dev/null)
if echo "$result" | grep -q 'User-Agent: MyAgent/1.0'; then
    pass "curl -A sets User-Agent"
else
    fail "curl -A User-Agent — expected MyAgent/1.0"
fi

echo "  ── Basic auth (-u) ──"
result=$("$MODBOX" curl -s -u "user:pass" "http://127.0.0.1:$PORT/echo" 2>/dev/null)
if echo "$result" | grep -q 'Authorization: Basic'; then
    pass "curl -u sets Authorization header"
else
    fail "curl -u Authorization — expected Authorization: Basic"
fi

echo "  ── Referer (-e) ──"
result=$("$MODBOX" curl -s -e "http://example.com" "http://127.0.0.1:$PORT/echo" 2>/dev/null)
if echo "$result" | grep -q 'Referer: http://example.com'; then
    pass "curl -e sets Referer header"
else
    fail "curl -e Referer — expected http://example.com"
fi

echo "  ── Cookie (-b) ──"
result=$("$MODBOX" curl -s -b "session=abc" "http://127.0.0.1:$PORT/echo" 2>/dev/null)
if echo "$result" | grep -q 'Cookie: session=abc'; then
    pass "curl -b sets Cookie header"
else
    fail "curl -b Cookie — expected session=abc"
fi

echo "  ── Output to file (-o) ──"
"$MODBOX" curl -s -o "$TMPDIR/output.txt" "http://127.0.0.1:$PORT/" 2>/dev/null
if [[ -f "$TMPDIR/output.txt" ]] && [[ "$(cat "$TMPDIR/output.txt")" == "hello world" ]]; then
    pass "curl -o writes response body to file"
else
    fail "curl -o output file — expected 'hello world' in file"
fi

echo "  ── Remote name (-O) ──"
( cd "$TMPDIR" && "$MODBOX" curl -s -O "http://127.0.0.1:$PORT/" 2>/dev/null )
if [[ -f "$TMPDIR/index.html" ]] && [[ "$(cat "$TMPDIR/index.html")" == "hello world" ]]; then
    pass "curl -O saves with remote filename"
else
    fail "curl -O remote filename — expected index.html with 'hello world'"
fi

echo "  ── Dump header (-D) ──"
"$MODBOX" curl -s -D "$TMPDIR/headers.txt" "http://127.0.0.1:$PORT/" 2>/dev/null > /dev/null
if grep -q '200 OK' "$TMPDIR/headers.txt" 2>/dev/null; then
    pass "curl -D dumps headers to file"
else
    fail "curl -D header dump — expected '200 OK' in header file"
fi

echo "  ── Include headers (-i) ──"
result=$("$MODBOX" curl -s -i "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -q 'HTTP/1.1 200 OK'; then
    pass "curl -i includes headers in output"
else
    fail "curl -i include headers — expected HTTP/1.1 200 OK"
fi

echo "  ── PUT request (-X PUT) ──"
result=$("$MODBOX" curl -s -X PUT -d "putdata" "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -q 'PUT body: putdata'; then
    pass "curl -X PUT sends PUT request"
else
    fail "curl -X PUT — expected PUT body"
fi

echo "  ── DELETE request (-X DELETE) ──"
result=$("$MODBOX" curl -s -X DELETE "http://127.0.0.1:$PORT/" 2>/dev/null)
if [[ "$result" == "DELETE ok" ]]; then
    pass "curl -X DELETE sends DELETE request"
else
    fail "curl -X DELETE — expected 'DELETE ok', got [$result]"
fi

echo "  ── -G with -d (query params) ──"
result=$("$MODBOX" curl -s -G -d "q=hello" "http://127.0.0.1:$PORT/echo" 2>/dev/null)
if echo "$result" | grep -q 'path=/echo?q=hello'; then
    pass "curl -G -d appends data as query params"
else
    fail "curl -G -d query params — expected path=/echo?q=hello, got [$result]"
fi

echo "  ── --data-urlencode ──"
result=$("$MODBOX" curl -s --data-urlencode "msg=hello world" "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -q 'msg=hello%20world'; then
    pass "curl --data-urlencode encodes special chars"
else
    fail "curl --data-urlencode — expected msg=hello%20world, got [$(echo "$result" | head -c 80)]"
fi

echo "  ── -w writeout http_code ──"
result=$("$MODBOX" curl -s -w '%{http_code}' "http://127.0.0.1:$PORT/" 2>/dev/null)
if [[ "$result" == "200" ]]; then
    pass "curl -w %{http_code} returns 200"
else
    fail "curl -w http_code — expected '200', got [$result]"
fi

echo "  ── -w writeout with newline ──"
result=$("$MODBOX" curl -s -w 'OK:%{http_code}\n' "http://127.0.0.1:$PORT/" 2>/dev/null)
if echo "$result" | grep -q 'OK:200'; then
    pass "curl -w with \\n format works"
else
    fail "curl -w newline format — expected 'OK:200', got [$result]"
fi

echo "  ── -L redirect following ──"
result=$("$MODBOX" curl -s -L "http://127.0.0.1:$PORT/redirect" 2>/dev/null)
if [[ "$result" == "final destination" ]]; then
    pass "curl -L follows redirect"
else
    fail "curl -L redirect — expected 'final destination', got [$result]"
fi

echo "  ── No -L means no redirect follow ──"
result=$("$MODBOX" curl -s "http://127.0.0.1:$PORT/redirect" 2>/dev/null)
if [[ -z "$result" ]] || echo "$result" | grep -q '302'; then
    pass "curl without -L does not follow redirect"
else
    fail "curl no -L redirect — expected empty or 302, got [$result]"
fi

echo "  ── -f fail on 404 ──"
"$MODBOX" curl -f "http://127.0.0.1:$PORT/404" 2>/dev/null
rc=$?
if [[ $rc -eq 22 ]]; then
    pass "curl -f returns exit code 22 on HTTP error"
else
    fail "curl -f — expected exit 22, got $rc"
fi

echo "  ── Without -f, 404 returns exit 0 ──"
"$MODBOX" curl -s "http://127.0.0.1:$PORT/404" 2>/dev/null > /dev/null
rc=$?
if [[ $rc -eq 0 ]]; then
    pass "curl without -f returns exit 0 on HTTP error"
else
    fail "curl no -f — expected exit 0, got $rc"
fi

echo "  ── -v verbose output ──"
result=$("$MODBOX" curl -v "http://127.0.0.1:$PORT/" 2>&1)
if echo "$result" | grep -q '> GET'; then
    pass "curl -v shows request headers"
else
    fail "curl -v — expected '> GET' in output"
fi

echo "  ── -s silent mode (no progress on non-TTY) ──"
result=$("$MODBOX" curl -s "http://127.0.0.1:$PORT/" 2>&1 1>/dev/null)
if [[ -z "$result" ]]; then
    pass "curl -s produces no stderr output"
else
    fail "curl -s — expected no stderr output, got [$(echo "$result" | head -c 80)]"
fi

echo "  ── Connection refused ──"
"$MODBOX" curl -s "http://127.0.0.1:1/" 2>/dev/null > /dev/null
rc=$?
if [[ $rc -ne 0 ]]; then
    pass "curl on connection refused returns non-zero exit ($rc)"
else
    fail "curl connection refused — expected non-zero exit, got $rc"
fi

# Cleanup
kill "$TEST_SERVER_PID" 2>/dev/null
wait "$TEST_SERVER_PID" 2>/dev/null

# Exit with proper code based on test results
if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi
exit 0
