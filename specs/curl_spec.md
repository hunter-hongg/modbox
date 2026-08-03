# ModBox Implementation Spec: curl Command

> **Reference**: GNU `curl` (curl.se). This spec covers a pragmatic subset appropriate for a BusyBox-style multi-call binary — the most commonly used HTTP/HTTPS features without the full protocol surface of the reference implementation.

## Problem Statement

modbox currently has no network-request command. Users on minimal containers or embedded systems that ship modbox instead of full coreutils must fall back to an external `curl` binary, defeating the purpose of a self-contained toolkit. Additionally, modbox's existing SSL dependency (`openssl`) can be reused to implement HTTPS without pulling in the large `libcurl` dependency tree.

## Solution

Implement `curl` as a modbox command that supports the most frequently used HTTP/HTTPS operations: GET, POST, PUT, DELETE, custom headers, basic authentication, output to file or stdout, progress meter, connection timeout, and redirect following. The implementation uses raw POSIX sockets + OpenSSL directly (no libcurl), keeping the dependency footprint minimal while covering the 80/20 of real-world curl usage.

## User Stories

1. As a developer, I want to run `curl https://example.com` so that I can fetch a web page from the command line.
2. As a developer, I want to run `curl -o file.json https://api.example.com/data` so that I can save the response body to a file.
3. As a developer, I want to run `curl -d '{"key":"value"}' -X POST https://api.example.com/submit` so that I can send JSON payloads to APIs.
4. As a developer, I want to run `curl -H "Authorization: Bearer token" https://api.example.com/secret` so that I can authenticate API requests.
5. As a developer, I want to run `curl -u user:pass https://secure.example.com` so that I can use basic authentication.
6. As a developer, I want to run `curl -I https://example.com` so that I can fetch only the response headers (HEAD request).
7. As a developer, I want to run `curl -s https://example.com` so that I can suppress the progress meter for scripting.
8. As a developer, I want to run `curl -w '%{http_code}' https://example.com` so that I can extract the HTTP status code for automation.
9. As a developer, I want to run `curl --max-time 10 https://slow.example.com` so that I can prevent requests from hanging forever.
10. As a developer, I want to run `curl -L https://example.com/redirect` so that I can follow HTTP redirects automatically.
11. As a developer, I want to run `curl -X DELETE https://api.example.com/resource/1` so that I can send DELETE requests.
12. As a developer, I want to run `curl -X PUT -d '{"name":"update"}' https://api.example.com/resource/1` so that I can send PUT requests.
13. As a sysadmin, I want to run `curl -v https://example.com` so that I can debug connection and header issues.
14. As a sysadmin, I want to run `curl -k https://self-signed.example.com` so that I can bypass certificate verification for internal services.
15. As a script writer, I want `curl --version` to display version information consistent with other modbox commands.
16. As a script writer, I want `curl --help` to display concise usage information and exit successfully.
17. As a developer, I want `curl` to read the URL from the first positional argument after all flags.
18. As a developer, I want `curl` to print the response body to stdout by default when no `-o` is given.
19. As a developer, I want `curl` to send `Content-Type: application/x-www-form-urlencoded` by default for POST body data.
20. As a developer, I want `curl` to send `Content-Type: application/json` when `-H "Content-Type: application/json"` is explicitly provided.
21. As a developer, I want `curl -D headers.txt` so that response headers are saved to a file while the body goes to stdout.
22. As a developer, I want `curl --retry 3 https://flaky.example.com` so that transient failures are automatically retried.
23. As a developer, I want `curl --retry-connrefused` so that connection-refused errors trigger a retry.
24. As a script writer, I want `curl` to exit with code 0 on successful responses (1xx, 2xx, 3xx) and non-zero on client/server errors (4xx, 5xx) when `-f` is used.
25. As a script writer, I want `curl -f` so that failed HTTP codes (4xx/5xx) cause a non-zero exit instead of printing the error body.
26. As a developer, I want `curl -G -d 'q=foo' https://api.example.com/search` so that data is appended as URL query parameters for GET requests.
27. As a developer, I want `curl --data-urlencode 'key=value with spaces' https://api.example.com` so that special characters are properly URL-encoded.
28. As a developer, I want `curl -A "Mozilla/5.0"` so that I can set a custom User-Agent header.
29. As a developer, I want `curl --cookie 'session=abc' https://example.com` so that I can send cookies.
30. As a developer, I want `curl --max-redirs 5 https://example.com` so that I can limit redirect chains.
31. As a developer, I want `curl --connect-timeout 5 https://example.com` so that I can limit time spent establishing the connection.
32. As a developer, I want `curl --fail-with-body` so that error responses include the body in the output (like real curl).
33. As a script writer, I want `curl -s -w '%{http_code}\n' https://example.com` so that I get a clean status code for parsing.
34. As a developer, I want `curl --no-buffer` or default behavior so that output is not line-buffered (matching real curl's unbuffered stdout for non-TTY).
35. As a developer, I want `curl -X PATCH -d '{"patch":true}' https://api.example.com/resource` so that I can send PATCH requests.

## Implementation Decisions

### Architecture

The implementation uses **raw POSIX sockets + OpenSSL (libssl/libcrypto)** directly — **no libcurl dependency**. OpenSSL is already a declared dependency in the project's `PKGS` line. This keeps the implementation self-contained and avoids adding a new system dependency.

### Modules to Build/Modify

- **New header**: `include/commands/curl.hpp` — declares `int curl_command(int argc, char** argv);`
- **New source**: `src/commands/curl.cpp` — implements the command
- **New source**: `src/commands/http_client.cpp` — HTTP request builder, response parser, redirect logic
- **New header**: `include/commands/http_client.hpp` — declares the HTTP client interface
- **Existing**: `src/main.cpp` — register the command via `REGISTER_COMMAND`
- **Existing**: `Makefile` — add `openssl` is already present; no new PKGS needed

### HTTP Client Design

The HTTP client is a stateless request builder + response parser:

```cpp
struct HttpResponse {
    int status_code = 0;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string final_url;
    double time_total = 0.0;
    double time_connect = 0.0;
};

struct CurlOptions {
    // Request method
    std::string method = "GET";
    // URL to fetch
    std::string url;
    // Output
    std::string output_file;          // -o
    std::string dump_header_file;     // -D
    // Headers
    std::vector<std::pair<std::string, std::string>> custom_headers;
    // Authentication
    std::string user;                 // -u user
    std::string password;             // -u :pass
    // Body
    std::string post_data;            // -d / --data
    bool data_urlencode = false;      // --data-urlencode
    bool get_with_data = false;       // -G (append data as query params)
    // Behavior
    bool silent = false;              // -s
    bool verbose = false;             // -v
    bool follow_redirects = false;    // -L
    int max_redirs = 20;              // --max-redirs
    bool insecure = false;            // -k
    double max_time = 0.0;            // --max-time
    double connect_timeout = 0.0;     // --connect-timeout
    int retry_count = 0;              // --retry
    bool retry_connrefused = false;   // --retry-connrefused
    bool fail_on_http_error = false;  // -f
    bool fail_with_body = false;      // --fail-with-body
    bool show_writeout = false;       // -w (enabled when -w is used)
    std::string writeout_format;      // -w FORMAT
    bool show_progress = true;        // true by default, false with -s
};
```

### URL Parsing

A minimal URL parser extracts `scheme`, `host`, `port`, `path`, and `query` from the URL string. Supports `http://` and `https://`. Invalid URLs produce an error on stderr and exit 1.

### TLS / HTTPS

When the URL scheme is `https://`, the implementation wraps the POSIX socket in an OpenSSL SSL connection (`SSL_new` + `SSL_set_fd` + `SSL_connect`). Certificate verification is skipped when `-k`/`--insecure` is set. The system's CA certificate store is used by default.

### Progress Meter

A simple ASCII progress bar is printed to stderr when the `Content-Length` header is present and output is to a TTY. Format:

```
  123  1003   123  1003  100   647  0:00:01  0:00:01 --:--:--   647
```

(Simplified 5-column format: downloaded / total / upload / download_speed / time.) When `-s` is used, no progress is printed. When output is redirected to a file (non-TTY), no progress is printed.

### Writeout (`-w`)

The `-w` / `--write-out` flag supports a subset of curl's format strings:

| Format String      | Meaning                                    |
|--------------------|--------------------------------------------|
| `%{http_code}`     | HTTP response status code                  |
| `%{size_download}` | Total bytes downloaded                   |
| `%{size_upload}`   | Total bytes uploaded                     |
| `%{time_total}`    | Total time in seconds                      |
| `%{time_connect}`  | Time to establish TCP connection           |
| `%{url_effective}` | Final URL after redirects                  |
| `%{content_type}`  | Response Content-Type                      |
| `%{num_redirects}` | Number of redirects followed               |
| `\n`               | Literal newline                            |

The format string is applied after the response is received; the result is printed to stdout (replacing the body unless `-o` was used).

### Redirect Handling

When `-L` is set, the client follows `301`, `302`, `303` redirects automatically. `307` and `308` preserve the original HTTP method; `301`/`302`/`303` may change POST to GET (matching curl's behavior). Each redirect is logged with `->` when `-v` is set. Redirects beyond `--max-redirs` (default 20) cause an error.

### Retry Logic

When `--retry N` is set, transient failures (connection refused, connection reset, timeout) are retried up to N times with a 1-second exponential backoff between attempts. `--retry-connrefused` enables retry on ECONNREFUSED specifically. Non-transient errors (4xx client errors when not using `-f`) are not retried.

### HTTP Methods

| Flag(s)                         | Method    |
|---------------------------------|-----------|
| (none, no `-d`)                 | GET       |
| `-I` / `--head`                 | HEAD      |
| `-X METHOD`                     | custom    |
| `-d` / `--data` (no `-X`)       | POST      |
| `-d` / `--data` with `-X GET`   | GET (with body, data as query if `-G`) |

### Content-Type Defaults

- POST with `-d` data and no explicit `Content-Type`: defaults to `application/x-www-form-urlencoded`
- POST with `-H "Content-Type: ..."`: uses the provided value
- POST with `--data-urlencode`: uses `application/x-www-form-urlencoded`

### Error Handling

| Condition                                          | stderr Output                                    | Exit Code |
|----------------------------------------------------|--------------------------------------------------|-----------|
| No URL provided                                    | `curl: no URL specified`                         | 1         |
| Unrecognized option                                | `curl: unrecognized option '...'`                | 1         |
| Invalid URL                                        | `curl: failed to parse URL '...'`                | 1         |
| DNS resolution failure                             | `curl: Could not resolve host '...'`             | 6         |
| Connection refused                                 | `curl: Failed to connect to '...' port 443: Connection refused` | 7  |
| TLS handshake failure (non-insecure)               | `curl: SSL connection error`                     | 35        |
| HTTP 4xx/5xx with `-f`                             | `curl: (22) The requested URL returned error: 404` | 22     |
| Timeout                                            | `curl: Operation timed out`                      | 28        |
| Too many redirects                                 | `curl: Too many redirects`                       | 47        |
| Cannot open output file                            | `curl: Failed to open file '...': ...`           | 1         |
| Empty response body for HEAD                       | (no body output)                                 | 0         |
| `--help`                                           | (usage text)                                     | 0         |
| `--version`                                        | (version text)                                   | 0         |

### Argument Interface

All options use argtable3. The following table summarizes supported flags for v1:

| Short | Long | Description |
|-------|------|-------------|
| `-X` | `--request=METHOD` | Specify request method |
| `-d` | `--data=DATA` | HTTP POST data |
| `--data-raw=DATA` | | Same as `-d` but no `-` interpretation |
| `--data-ascii=DATA` | | Same as `-d` (alias) |
| `--data-binary=DATA` | | Same as `-d` (alias) |
| `-G` | `--get` | Append POST data to URL as query string |
| `--data-urlencode=DATA` | | URL-encode POST data |
| `-H` | `--header=HEADER` | Pass custom header |
| `-I` | `--head` | Show headers only (HEAD request) |
| `-o` | `--output=FILE` | Write to file |
| `-O` | `--remote-name` | Save as remote filename |
| `-D` | `--dump-header=FILE` | Save headers to file |
| `-u` | `--user=USER:PASSWORD` | Basic authentication |
| `-A` | `--user-agent=AGENT` | Set User-Agent |
| `-e` | `--referer=REFERER` | Set Referer header |
| `-b` | `--cookie=COOKIE` | Send cookie |
| `-c` | `--cookie-jar=FILE` | Save cookies to file (out of scope v1 — warn + ignore) |
| `-s` | `--silent` | Silent mode |
| `-v` | `--verbose` | Verbose output |
| `-i` | `--include` | Include HTTP headers in output |
| `-k` | `--insecure` | Allow insecure SSL connections |
| `-L` | `--location` | Follow redirects |
| `--max-redirs=NUM` | | Maximum redirects (default 20) |
| `-m` | `--max-time=SECONDS` | Maximum time |
| `--connect-timeout=SECONDS` | | Connection timeout |
| `--retry=NUM` | | Retry count on transient errors |
| `--retry-connrefused` | | Retry on connection refused |
| `-f` | `--fail` | Fail on HTTP 4xx/5xx |
| `--fail-with-body` | | Fail with body on HTTP 4xx/5xx |
| `-w` | `--write-out=FORMAT` | Output format after transfer |
| `--no-buffer` | | Disable buffering (default for non-TTY) |
| `--progress-bar` | | Simple progress bar |
| `--compressed` | | Request compressed response (out of scope v1 — warn + ignore uncompressed) |
| `--basic` | | Use HTTP Basic auth (default, no-op flag) |
| `--basic` | | Auth method selector (no-op, basic is default) |
| `--proxy=HOST` | | HTTP proxy (out of scope v1 — reject with error) |
| `--proxy-user` | | Proxy auth (out of scope v1 — reject with error) |
| `-T` | `--upload-file=FILE` | Upload file (PUT from file) |
| `--next` | | Separate URL + options (out of scope v1 — reject) |
| `--url` | | URL (same as positional arg) |
| `--global-ftp-coding` | | No-op (compatibility) |
| `--version` | | Version info |
| `--help` | | Help |

**Mutual exclusion constraints**:
- `-o` and `-O` are mutually exclusive
- `-I` and `-d`/`--data*` are incompatible (HEAD with body is unusual)
- `-s` suppresses progress meter; `--progress-bar` conflicts with `-s`

### File Structure

```
include/commands/curl.hpp          — public interface
include/commands/http_client.hpp   — HTTP client types and functions
src/commands/curl.cpp              — argument parsing, command entry point
src/commands/http_client.cpp       — socket/SSL I/O, request building, response parsing
src/main.cpp                       — REGISTER_COMMAND("curl", curl_command, ...)
```

### Build System

No new `PKGS` entries needed — `openssl` is already in the `PKGS` line:

```makefile
PKGS := argtable3 ftxui openssl libselinux libacl
```

No Makefile changes required.

## Testing Decisions

### Test Approach

Use the existing test framework at `tests/run_tests.sh`. Create `tests/test_curl.sh`.

### Test Seams

1. **Argument validation seam** (no network required): Tests for `--help`, `--version`, unknown options, conflicting flags, missing URL. This is the highest seam and requires no network access.
2. **URL parsing seam** (no network required): Tests with various URL strings to verify correct parsing of scheme, host, port, path, query.
3. **Local HTTP server seam** (requires a test HTTP server): Spin up a minimal HTTP server on a random port using Python's `http.server` or a simple C server, then test curl against it. This seam covers actual HTTP request/response behavior.
4. **Progress/output seam** (requires local server): Tests for `-o`, `-D`, `-w`, `-s`, `-v` behavior against the local server.
5. **Redirect seam** (requires local server): Tests for `-L` redirect following.
6. **Error seam** (no network required): Tests for invalid URLs, DNS failures, connection refused against `127.0.0.1:1` (guaranteed to fail).

The primary testing seam is argument validation + URL parsing (zero network) and local HTTP server tests (controlled environment).

### Test Cases to Implement

1. **`--help`**: `assert_cmd_pat 'Usage: curl' curl --help` — exit 0.
2. **`--version`**: `assert_cmd_pat 'curl \(modbox\) 1\.0' curl --version` — exit 0.
3. **No URL**: `assert_cmd_pat_stderr 'no URL specified' curl` — exit non-zero.
4. **Unknown option**: `assert_cmd_pat_stderr 'unrecognized option' curl --foo` — exit non-zero.
5. **Invalid URL**: `assert_cmd_pat_stderr 'failed to parse URL' curl '://bad'` — exit non-zero.
6. **GET against local server**: Start a Python HTTP server, fetch `/`, verify response body matches.
7. **POST against local server**: Send `-d 'key=value'`, verify server receives correct body and Content-Type.
8. **Custom header**: Send `-H "X-Test: value"`, verify server echoes the header.
9. **Output to file (`-o`)**: Fetch URL, verify file content matches response body.
10. **Verbose mode (`-v`)**: Fetch URL, verify stderr contains `> GET` and `< HTTP`.
11. **Silent mode (`-s`)**: Fetch URL, verify no progress output on stderr.
12. **HEAD request (`-I`)**: Fetch with `-I`, verify only headers in output, no body.
13. **Basic auth (`-u`)**: Fetch protected URL, verify 401 without auth, 200 with auth.
14. **Redirect (`-L`)**: Server returns 302 redirect, verify curl follows and returns final content.
15. **Redirect without `-L`**: Server returns 302, verify curl prints redirect location and exits 0.
16. **Writeout (`-w`)**: `curl -w '%{http_code}' URL` — verify stdout is just the status code.
17. **`-f` flag**: Server returns 404, `curl -f` exits non-zero; without `-f` exits 0.
18. **Timeout**: `curl --max-time 0.1 http://127.0.0.1:1` — verify timeout error and exit 28.
19. **Connection refused**: `curl http://127.0.0.1:1` — verify connection error and exit 7.
20. **Query params with `-G -d`**: `curl -G -d 'q=hello' http://server/echo` — verify URL contains `?q=hello`.
21. **`--data-urlencode`**: `curl --data-urlencode 'msg=hello world' http://server/echo` — verify URL-encoded body.
22. **User-Agent**: `curl -A 'modbox-curl/1.0' http://server/echo` — verify User-Agent header received.
23. **Conflicting `-o` and `-O`**: `assert_cmd_pat_stderr 'conflicts' curl -o out -O URL`.
24. **Exit codes**: Verify exit code 0 for success, 22 for HTTP error with `-f`.

### Existing Test Patterns

Examine `tests/test_getenforce.sh` and `tests/test_audit2allow.sh` for assertion style. Use `assert_cmd_pat`, `assert_cmd_pat_stderr`, and `assert_cmd_not_pat` helpers from `tests/framework.sh`.

For local HTTP server tests, use Python's built-in `http.server`:

```bash
# Spin up a test server in the background
python3 -c "
import http.server, socketserver, json, sys
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'hello')
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *a): pass
with socketserver.TCPServer(('127.0.0.1', 0), H) as s:
    print(s.server_address[1])
    s.serve_forever()
" &
```

Capture the port and use it in tests. Teardown with `kill` in a trap.

### Conditional Tests

Tests requiring a local HTTP server should be wrapped in a conditional:

```bash
if ! command -v python3 >/dev/null 2>&1; then
    echo "  SKIP — python3 not available for test HTTP server"
    return
fi
```

Tests requiring network access to external hosts should be skipped in CI environments without internet.

## Out of Scope

- **FTP, FTPS, SFTP, SCP**: Protocol support beyond HTTP/HTTPS is out of scope for v1.
- **WebSocket (`ws`/`wss`)**: Out of scope.
- **HTTP/2**: The implementation uses HTTP/1.1 only. No `--http2` support.
- **Proxy support** (`--proxy`, `--proxy-user`, `--noproxy`): Out of scope for v1 — these flags are accepted but produce an error.
- **Cookie jar** (`-c`): Out of scope — warn and ignore.
- **Multiplexed transfers** (`--next`, multiple URLs): Out of scope — reject with error.
- **Form posting with `--form`**: Out of scope for v1 — `--data` covers URL-encoded forms.
- **Resume partial download** (`-C` / `--continue-at`): Out of scope.
- **Compression negotiation** (`--compressed`): Out of scope — v1 accepts the flag but does not send `Accept-Encoding` or decode responses.
- **IP resolution options** (`--resolve`, `--dns servers`): Out of scope.
- **SSL client certificates** (`--cert`, `--key`): Out of scope.
- **Progress bar with `--progress-bar`**: A simple text progress is implemented; a full ETA/percentage bar is out of scope for v1.
- **curl's full write-out variables**: Only the most common `%{http_code}`, `%{size_download}`, `%{time_total}`, `%{time_connect}`, `%{url_effective}`, `%{content_type}`, `%{num_redirects}` are supported.
- **Upload from file** (`-T`): Out of scope for v1.
- **Resume on server support** (`-C -`): Out of scope.

## Further Notes

- The reference `curl` is a ~200K-line C project supporting dozens of protocols. This spec targets the ~20% of features used in ~80% of real-world command-line usage.
- OpenSSL is already a project dependency (`PKGS := ... openssl ...`), so no new build dependencies are introduced.
- The local test HTTP server approach (Python `http.server`) mirrors how other projects test HTTP clients without external dependencies.
- `curl` is invoked as `modbox curl` or directly as a symlink `curl`, consistent with modbox's multi-call binary architecture.
- The implementation does not use libcurl to avoid the large dependency tree (libcurl pulls in nghttp2, brotli, zlib, etc.) and to match modbox's philosophy of minimal dependencies.
- The `-w` writeout feature uses a simple format-string interpreter — no regex or complex parsing.
- The progress meter prints to stderr; the response body prints to stdout; these are independent streams, matching real curl behavior.
- When `-o` is used, the progress meter still prints to stderr but the body goes to the file, not stdout.
- The `--data-urlencode` flag URL-encodes both the key and value, matching curl's behavior (e.g., spaces become `+`).
- For `--retry`, the first attempt counts as attempt 0; retries happen on attempts 1 through N.
- On Windows compatibility is not a goal; the implementation targets POSIX/Linux only (consistent with the rest of modbox).
