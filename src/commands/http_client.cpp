#include "commands/http_client.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// ── URL Parsing ─────────────────────────────────────────────────────────────

bool parse_url(const char* url_str, UrlParts& out) {
    if (!url_str || url_str[0] == '\0') return false;

    std::string s(url_str);
    out = UrlParts{};

    size_t scheme_end = s.find("://");
    if (scheme_end == std::string::npos) return false;
    out.scheme = s.substr(0, scheme_end);
    if (out.scheme != "http" && out.scheme != "https") return false;
    size_t rest_start = scheme_end + 3;

    size_t path_start = s.find('/', rest_start);
    if (path_start == std::string::npos) path_start = s.size();

    std::string authority = s.substr(rest_start, path_start - rest_start);
    size_t at_pos = authority.find('@');
    if (at_pos != std::string::npos) authority = authority.substr(at_pos + 1);

    size_t host_end = authority.find(':');
    if (host_end != std::string::npos) {
        out.host = authority.substr(0, host_end);
        std::string port_str = authority.substr(host_end + 1);
        if (!port_str.empty()) out.port = std::stoi(port_str);
    } else {
        out.host = authority;
    }
    if (out.host.empty()) return false;

    std::string remainder = (path_start < s.size()) ? s.substr(path_start) : "/";
    size_t query_pos = remainder.find('?');
    size_t frag_pos = remainder.find('#');

    if (query_pos != std::string::npos) {
        out.path = remainder.substr(0, query_pos);
        size_t end = (frag_pos != std::string::npos) ? frag_pos : remainder.size();
        out.query = remainder.substr(query_pos + 1, end - query_pos - 1);
        if (frag_pos != std::string::npos) out.fragment = remainder.substr(frag_pos + 1);
    } else if (frag_pos != std::string::npos) {
        out.path = remainder.substr(0, frag_pos);
        out.fragment = remainder.substr(frag_pos + 1);
    } else {
        out.path = remainder;
    }

    if (out.path.empty()) out.path = "/";
    if (out.port == 0) out.port = default_port(out.scheme);
    return true;
}

int default_port(const std::string& scheme) {
    return (scheme == "https") ? 443 : 80;
}

std::string build_request_target(const UrlParts& url) {
    std::string target = url.path;
    if (!url.query.empty()) target += "?" + url.query;
    return target;
}

// ── URL Encoding ─────────────────────────────────────────────────────────────

static const char hex_chars[] = "0123456789ABCDEF";

std::string url_encode(const std::string& input) {
    std::string result;
    result.reserve(input.size() + 10);
    for (unsigned char c : input) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex_chars[c >> 4];
            result += hex_chars[c & 0x0F];
        }
    }
    return result;
}

// ── Low-level I/O ───────────────────────────────────────────────────────────

static ssize_t recv_timeout(int fd, void* buf, size_t len, double timeout_secs) {
    if (timeout_secs <= 0) return recv(fd, buf, len, 0);
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    struct timeval tv;
    tv.tv_sec = (time_t)timeout_secs;
    tv.tv_usec = (long)((timeout_secs - tv.tv_sec) * 1000000);
    int ret = select(fd + 1, &readfds, nullptr, nullptr, &tv);
    if (ret <= 0) return -1;
    return recv(fd, buf, len, 0);
}

static bool send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

static int connect_with_timeout(const std::string& host, int port, double timeout) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0) return -1;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    if (::connect(sock, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
        close(sock); freeaddrinfo(res); return -1;
    }

    if (timeout > 0) {
        fd_set wset;
        FD_ZERO(&wset); FD_SET(sock, &wset);
        struct timeval tv;
        tv.tv_sec = (time_t)timeout;
        tv.tv_usec = (long)((timeout - tv.tv_sec) * 1000000);
        if (select(sock + 1, nullptr, &wset, nullptr, &tv) <= 0) {
            close(sock); freeaddrinfo(res); errno = ETIMEDOUT; return -1;
        }
        int sockerr; socklen_t len = sizeof(sockerr);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockerr, &len);
        if (sockerr != 0) { close(sock); freeaddrinfo(res); errno = sockerr; return -1; }
    }

    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    freeaddrinfo(res);
    return sock;
}

// ── SSL ─────────────────────────────────────────────────────────────────────

static SSL* ssl_handshake(int fd, bool insecure) {
    SSL_library_init();
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return nullptr;
    if (insecure) SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    SSL* ssl = SSL_new(ctx);
    if (!ssl) { SSL_CTX_free(ctx); return nullptr; }
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl); SSL_CTX_free(ctx); errno = ECONNRESET; return nullptr;
    }
    SSL_CTX_free(ctx);
    return ssl;
}

// ── HTTP Parsing ─────────────────────────────────────────────────────────────

static bool parse_status_line(const char* line, size_t len, int& status_code, std::string& status_text) {
    // Format: "HTTP/1.1 200 OK" or "HTTP/1.0 200 OK"
    if (len < 12 || strncmp(line, "HTTP/", 5) != 0) return false;
    const char* p = line + 5;
    // Skip version (e.g., "1.1" or "1.0")
    while (*p && *p != ' ') ++p;
    // Skip spaces
    while (*p == ' ') ++p;
    // Now p should point to the status code
    char* end = nullptr;
    long code = strtol(p, &end, 10);
    if (end == p) return false;
    status_code = (int)code;
    if (*end == ' ') status_text = end + 1;
    else status_text = "";
    return true;
}

static void parse_header_line(const char* line, size_t len,
                               std::unordered_map<std::string, std::string>& headers) {
    const char* colon = (const char*)memchr(line, ':', len);
    if (!colon) return;
    std::string key(line, colon - line);
    std::string val(colon + 1, line + len - colon - 1);
    // Trim
    while (!key.empty() && key.back() == ' ') key.pop_back();
    while (!key.empty() && key.front() == ' ') key.erase(key.begin());
    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    headers[key] = val;
}

// ── Main request ─────────────────────────────────────────────────────────────

// ── HTTP Response Parser ─────────────────────────────────────────────────────

static bool read_response(int fd, SSL* ssl, const CurlOptions& opts, HttpResponse& response) {
    // Read entire response into a buffer first
    std::string raw;
    char rbuf[4096];
    while (true) {
        ssize_t n;
        if (ssl) {
            n = SSL_read(ssl, rbuf, sizeof(rbuf));
        } else {
            n = recv(fd, rbuf, sizeof(rbuf), 0);
        }
        if (n <= 0) break;
        raw.append(rbuf, n);
    }

    if (raw.empty()) {
        return false;
    }

    // Find end of headers
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return false;
    }

    // Parse status line
    std::string header_block = raw.substr(0, header_end);
    size_t nl = header_block.find('\n');
    if (nl != std::string::npos) {
        std::string status_line = header_block.substr(0, nl);
        // Strip \r
        if (!status_line.empty() && status_line.back() == '\r') status_line.pop_back();
        // Parse: "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
        int status_code = 0;
        std::string status_text;
        if (parse_status_line(status_line.c_str(), status_line.size(), status_code, status_text)) {
            response.status_code = status_code;
            response.status_text = status_text;
        } else {
            response.status_code = 0;
            response.status_text = "";
        }
    }

    // Parse headers
    std::istringstream iss(header_block);
    std::string line;
    while (std::getline(iss, line)) {
        // Strip \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            while (!key.empty() && key.back() == ' ') key.pop_back();
            while (!val.empty() && val.front() == ' ') val.erase(val.begin());
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            response.headers[key] = val;
        }
    }

    // Get body (everything after \r\n\r\n)
    size_t body_start = header_end + 4;
    response.body = raw.substr(body_start);
    response.size_download = (long)response.body.size();

    return true;
}

int http_request(const CurlOptions& opts, HttpResponse& response) {
    UrlParts url;
    if (!parse_url(opts.url.c_str(), url)) {
        fprintf(stderr, "curl: failed to parse URL '%s'\n", opts.url.c_str());
        return 1;
    }

    std::string current_url = opts.url;
    int redirect_count = 0;
    std::string current_method = opts.method;
    std::string current_body = opts.post_data;

    auto get_time = [](double& t) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        t = tv.tv_sec + tv.tv_usec / 1000000.0;
    };

    double t_start, t_connect;
    get_time(t_start);

    int sock = -1;
    SSL* ssl = nullptr;
    int retry_delay = 1;

    for (int attempt = 0; ; ++attempt) {
        if (attempt > 0) { usleep(retry_delay * 1000000); retry_delay *= 2; }
        if (sock >= 0) close(sock);
        ssl = nullptr;
        sock = connect_with_timeout(url.host, url.port, opts.connect_timeout);
        if (sock < 0) {
            bool retryable = (errno == ECONNREFUSED && opts.retry_connrefused) ||
                             (errno == ETIMEDOUT) || (errno == ECONNRESET) ||
                             (errno == ECONNABORTED);
            if (!retryable || attempt >= opts.retry_count) {
                if (errno == ECONNREFUSED)
                    fprintf(stderr, "curl: Failed to connect to '%s' port %d: Connection refused\n",
                            url.host.c_str(), url.port);
                else if (errno == ETIMEDOUT)
                    fprintf(stderr, "curl: Operation timed out\n");
                else
                    fprintf(stderr, "curl: Failed to connect to '%s' port %d: %s\n",
                            url.host.c_str(), url.port, strerror(errno));
                return 7;
            }
            continue;
        }
        get_time(t_connect);

        if (url.scheme == "https") {
            ssl = ssl_handshake(sock, opts.insecure);
            if (!ssl) {
                if (opts.retry_count > 0 && attempt < opts.retry_count) { close(sock); continue; }
                fprintf(stderr, "curl: SSL connection error\n");
                return 35;
            }
        }

        // Build request
        std::string target = build_request_target(url);
        if (opts.get_with_data && !current_body.empty() && current_body.find('=') != std::string::npos) {
            std::string extra = (!url.query.empty() ? "&" : "") + current_body;
            target = url.path + "?" + extra;
        }

        std::string host_header = url.host;
        if (url.port != default_port(url.scheme)) host_header += ":" + std::to_string(url.port);

        // Check content-type
        bool has_ct = false;
        for (auto& h : opts.custom_headers) {
            std::string k = h.first;
            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
            if (k == "content-type") { has_ct = true; break; }
        }

        std::string req;
        req += current_method + " " + target + " HTTP/1.1\r\n";
        req += "Host: " + host_header + "\r\n";
        req += "Connection: close\r\n";
        req += "User-Agent: modbox-curl/1.0\r\n";
        for (auto& h : opts.custom_headers)
            req += h.first + ": " + h.second + "\r\n";
        if (!opts.head_only && !current_body.empty() && !has_ct)
            req += "Content-Type: application/x-www-form-urlencoded\r\n";
        if (!current_body.empty())
            req += "Content-Length: " + std::to_string(current_body.size()) + "\r\n";
        req += "\r\n";

        // Verbose output
        if (opts.verbose) {
            fprintf(stderr, "> %s %s HTTP/1.1\n", current_method.c_str(), target.c_str());
            fprintf(stderr, "> Host: %s\n", host_header.c_str());
            fprintf(stderr, "> User-Agent: modbox-curl/1.0\n");
            for (auto& h : opts.custom_headers)
                fprintf(stderr, "> %s: %s\n", h.first.c_str(), h.second.c_str());
            if (!opts.head_only && !current_body.empty() && !has_ct)
                fprintf(stderr, "> Content-Type: application/x-www-form-urlencoded\n");
            if (!current_body.empty())
                fprintf(stderr, "> Content-Length: %zu\n", current_body.size());
            if (!current_body.empty())
                fprintf(stderr, ">\n> %s\n", current_body.c_str());
            fprintf(stderr, ">\n");
        }

        auto do_send = [&](const char* data, size_t len) -> bool {
            size_t sent = 0;
            while (sent < len) {
                ssize_t n;
                if (ssl) {
                    n = SSL_write(ssl, data + sent, len - sent);
                    if (n <= 0) return false;
                } else {
                    n = send(sock, data + sent, len - sent, 0);
                    if (n <= 0) return false;
                }
                sent += n;
            }
            return true;
        };

        if (!do_send(req.c_str(), req.size())) {
            if (ssl) SSL_shutdown(ssl);
            close(sock);
            if (attempt < opts.retry_count) continue;
            fprintf(stderr, "curl: Failed to send request\n");
            return 56;
        }

        if (!current_body.empty()) {
            if (!do_send(current_body.c_str(), current_body.size())) {
                if (ssl) SSL_shutdown(ssl); close(sock);
                if (attempt < opts.retry_count) continue;
                fprintf(stderr, "curl: Failed to send request\n");
                return 56;
            }
        }

        // Read response
        response.status_code = 0;
        response.headers.clear();
        response.body.clear();
        response.final_url = current_url;
        response.num_redirects = redirect_count;

        if (!read_response(sock, ssl, opts, response)) {
            if (ssl) SSL_shutdown(ssl);
            close(sock);
            fprintf(stderr, "curl: Failed to read response\n");
            return 56;
        }

        // Verbose output for response
        if (opts.verbose) {
            fprintf(stderr, "< HTTP/1.1 %d %s\n", response.status_code, response.status_text.c_str());
            for (auto& h : response.headers) {
                fprintf(stderr, "< %s: %s\n", h.first.c_str(), h.second.c_str());
            }
            fprintf(stderr, "<\n");
        }

        // Handle redirect
        bool is_redirect = (response.status_code == 301 || response.status_code == 302 ||
                           response.status_code == 303 || response.status_code == 307 ||
                           response.status_code == 308);
        if (is_redirect && opts.follow_redirects) {
            auto loc = response.headers.find("location");
            if (loc != response.headers.end()) {
                std::string loc_url = loc->second;
                if (!loc_url.empty() && loc_url[0] == '/') {
                    loc_url = url.scheme + "://" + url.host +
                              (url.port != default_port(url.scheme) ? ":" + std::to_string(url.port) : "") + loc_url;
                }
                if (redirect_count >= opts.max_redirs) {
                    if (ssl) SSL_shutdown(ssl); close(sock);
                    fprintf(stderr, "curl: Too many redirects\n");
                    return 47;
                }
                if (opts.verbose)
                    fprintf(stderr, "curl: %d %s -> %s\n", response.status_code, current_url.c_str(), loc_url.c_str());
                redirect_count++;
                current_url = loc_url;
                parse_url(loc_url.c_str(), url);
                if (response.status_code == 301 || response.status_code == 302 || response.status_code == 303) {
                    if (current_method == "POST" || current_method == "PUT" || current_method == "DELETE") {
                        current_method = "GET";
                        current_body.clear();
                    }
                }
                if (ssl) SSL_shutdown(ssl);
                close(sock);
                // Reset attempt counter for redirects - don't count as retry
                attempt = -1; // Will become 0 after ++attempt
                continue;
            }
        }

        if (ssl) SSL_shutdown(ssl);
        close(sock);
        break;
    }

    double t_end;
    get_time(t_end);
    response.time_total = t_end - t_start;
    response.time_connect = t_connect - t_start;

    return 0;
}
