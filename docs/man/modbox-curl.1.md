% MODBOX-CURL(1) modbox | User Commands
% modbox project
% 2026-08-16

# NAME

modbox-curl - transfer data from or to a server

# SYNOPSIS

**modbox curl** [*OPTION*]... [**URL**]

# DESCRIPTION

A Curl-like HTTP client for modbox. Supports common HTTP methods,
authentication, cookies, and various output control options.

When no URL is provided, curl prints an error. When **URL** is provided
without options, the content is written to standard output.

# OPTIONS

## Request options

**-X**, **--request=METHOD**
:   Specify the request method (GET, POST, PUT, DELETE, HEAD, etc.).
    Default is GET for URLs without data, POST when data is present.

**-d**, **--data=DATA**
:   HTTP POST data. Content-Type defaults to `application/x-www-form-urlencoded`.

**--data-raw=DATA**
:   Same as **--data**, but does not interpret backslash escapes.

**--data-ascii=DATA**
:   Same as **--data**, but strips trailing carriage returns.

**--data-binary=DATA**
:   Same as **--data**, but sends data exactly as provided.

**--data-urlencode=DATA**
:   URL-encodes the data before sending. Format: `name=value`.

**-G**, **--get**
:   Appends **--data** to the URL as query parameters instead of sending
    as POST body.

**-H**, **--header=HEADER**
:   Custom header to send. Format: `Header-Name: value`.
    Can be specified multiple times.

## Output control

**-o**, **--output=FILE**
:   Write response to FILE instead of stdout.

**-O**, **--remote-name**
:   Save response to a file named after the remote file.

**-D**, **--dump-header=FILE**
:   Write response headers to FILE.

**-i**, **--include**
:   Include HTTP headers in the output.

**-I**, **--head**
:   Show headers only (sends HEAD request).

**-w**, **--write-out=FORMAT**
:   Output FORMAT after transfer. Supported variables:
    `%{http_code}`, `%{size_download}`, `%{size_upload}`,
    `%{time_total}`, `%{time_namelookup}`, `%{time_connect}`,
    `%{time_appconnect}`, `%{time_pretransfer}`,
    `%{time_starttransfer}`, `%{num_connects}`,
    `%{url_effective}`, `%{remote_ip}`, `%{remote_port}`,
    `%{local_ip}`, `%{local_port}`,
    `\n` for newline.

## Connection options

**-m**, **--max-time=SECONDS**
:   Maximum time allowed for the transfer.

**--connect-timeout=SECONDS**
:   Maximum time for connection phase only.

**--max-redirs=NUM**
:   Maximum number of redirects allowed (default: 20).

**--retry=NUM**
:   Number of retries on transient errors.

**--retry-connrefused**
:   Retry on connection refused errors.

## Security options

**-k**, **--insecure**
:   Allow insecure SSL connections (skip certificate verification).

**-L**, **--location**
:   Follow redirects.

**-f**, **--fail**
:   Fail silently on HTTP errors (4xx/5xx). Exit code is non-zero.

**--fail-with-body**
:   Like **--fail**, but include the response body in output.

**-u**, **--user=USER:PASSWORD**
:   Basic authentication.

## Identity options

**-A**, **--user-agent=AGENT**
:   Set the User-Agent header.

**-e**, **--referer=REFERER**
:   Set the Referer header.

**-b**, **--cookie=COOKIE**
:   Send cookie data.

## Verbose/Silent mode

**-s**, **--silent**
:   Silent mode. Do not show progress meter or error messages.

**-v**, **--verbose**
:   Verbose output. Shows protocol details and headers.

**-q**, **--question**
:   Quiet mode. Suppresses all output on errors.

## Transfer options

**--progress-bar**
:   Show progress as a simple bar instead of the default meter.

**--no-buffer**
:   Disable buffering of the output.

**--compressed**
:   Request compressed response (gzip/deflate).

## Information

**-V**, **--version**
:   Output version information and exit.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Get a web page
modbox curl https://example.com

# POST data
modbox curl -X POST -d "name=value" https://api.example.com/data

# POST JSON
modbox curl -X POST -H "Content-Type: application/json" -d '{"key":"value"}' https://api.example.com/data

# Download and save to file
modbox curl -o output.pdf https://example.com/document.pdf

# Follow redirects
modbox curl -L https://short.url

# With authentication
modbox curl -u user:pass https://protected.example.com

# Show headers
modbox curl -I https://example.com

# With custom headers
modbox curl -H "Authorization: Bearer token" https://api.example.com

# Print response code
modbox curl -w "%{http_code}" -s https://example.com

# Retry on failure
modbox curl --retry 3 --retry-connrefused https://flaky.example.com

# Timeout after 30 seconds
modbox curl -m 30 https://slow.example.com
```

# NOTES

- This is a lightweight implementation and may not support all curl features.
- SSL/TLS support depends on the OpenSSL library being available.
- The `--data-urlencode` option URL-encodes the value portion of `name=value`.
- When using `-w`, the format string is printed after the transfer completes.
- Progress bar can be disabled with `-s` (silent mode).

## Differences from GNU curl

The following GNU curl features are **not implemented**:

- FTP/FTPS/SCP/SFTP protocols
- SMTP/IMAP/POP3 protocols
- Proxy support (HTTP CONNECT tunneling)
- Form upload (`-F` / `--form`)
- Resume partial downloads (`-C` / `--continue-at`)
- Multithreaded transfers (`--parallel`)
- HTTP/3 support

# EXIT STATUS

`0`
:   Success.

`1`
:   Unsupported protocol.

`2`
:   Failed to initialize.

`3`
:   URL malformed.

`4`
:   Character conversion failed.

`5`
:   FTP parse error.

`6`
:   Could not resolve host.

`7`
:   Failed to connect to host.

`8`
:   Server rejected the request.

`9`
:   Access denied.

`35`
:   SSL connection error.

`52`
:   Empty reply from server.

`56`
:   Failure in receiving network data.

`60`
:   SSL certificate problem.

`67`
:   Authentication failed.

`77`
:   Problem with reading the SSL CA cert.

`78`
:   Resource lock error.

`92`
:   HTTP/2 stream error.

`28`
:   Operation timeout.

`55`
:   Failed to send POST data.

`58`
:   Could not parse proxy response.

`63`
:   Exceeded maximum redirect count.

`66`
:   Failed to open file.

`75`
:   Can ERE reset.

`76`
:   Initial SSL connect error.

`83`
:   Notification message too large.

`91`
:   SSL parameter malformed.

`93`
:   Invalid request.

`96`
:   Bad socket.

`97`
:   Broken pipe.

`98`
:   Certificate verification failed.

`99`
:   SSL version/algorithm mismatch.

# SEE ALSO

**modbox-wget**(1), **modbox-curl**(1), **modbox**(1)
