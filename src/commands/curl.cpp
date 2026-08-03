#include "commands/curl.hpp"

#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "commands/http_client.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/arg_util.hpp"

// ── Progress meter ──────────────────────────────────────────────────────────

static void print_progress(long downloaded, long total, long uploaded, long upload_total,
                            double time_total, double time_spent,
                            bool silent, bool progress_bar) {
    if (silent) return;
    if (!isatty(STDERR_FILENO)) return;
    if (!progress_bar && total == 0) return;

    char time_str[32];
    auto fmt_time = [](double s) -> const char* {
        static char buf[32];
        long h = (long)s / 3600;
        long m = ((long)s % 3600) / 60;
        long sec = (long)s % 60;
        if (h > 0)
            snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", h, m, sec);
        else
            snprintf(buf, sizeof(buf), "%02ld:%02ld", m, sec);
        return buf;
    };

    auto speed_str = [](long bytes, double secs) -> const char* {
        static char buf[32];
        if (secs <= 0) { snprintf(buf, sizeof(buf), "   -"); return buf; }
        double bps = (double)bytes / secs;
        if (bps >= 1024 * 1024)
            snprintf(buf, sizeof(buf), "%6.1fM", bps / (1024 * 1024));
        else if (bps >= 1024)
            snprintf(buf, sizeof(buf), "%6.1fk", bps / 1024);
        else
            snprintf(buf, sizeof(buf), "%7.0f", bps);
        return buf;
    };

    if (progress_bar) {
        // Simple progress bar
        int bar_width = 40;
        int filled = total > 0 ? (int)((double)downloaded / total * bar_width) : 0;
        if (filled > bar_width) filled = bar_width;
        std::string bar_filled(filled, '#');
        std::string bar_empty(bar_width - filled, ' ');
        fprintf(stderr, "\r[%s] %ld/%ld %s %s  ",
                (bar_filled + bar_empty).c_str(),
                downloaded, total, speed_str(downloaded, time_spent), fmt_time(time_spent));
        fflush(stderr);
        if (downloaded >= total) fprintf(stderr, "\n");
    } else {
        // Simple line output
        fprintf(stderr, "\r%7ld  %7ld  %7ld  %7ld  %6s  %6s  %s  %s",
                downloaded, total, uploaded,
                total > 0 ? uploaded : 0,
                speed_str(downloaded, time_spent),
                speed_str(uploaded, time_spent),
                fmt_time(time_spent), fmt_time(time_spent));
        fflush(stderr);
    }
}

static void clear_progress() {
    fprintf(stderr, "\r\033[K");
    fflush(stderr);
}

// ── Writeout formatting ─────────────────────────────────────────────────────

static std::string format_writeout(const std::string& fmt, const HttpResponse& resp,
                                    long size_download, long size_upload) {
    std::string result;
    result.reserve(fmt.size() * 2);

    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '\\' && i + 1 < fmt.size() && fmt[i + 1] == 'n') {
            result += '\n';
            ++i;
        } else if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == '{') {
            size_t end = fmt.find('}', i + 2);
            if (end == std::string::npos) {
                result += fmt[i];
                continue;
            }
            std::string var = fmt.substr(i + 2, end - i - 2);

            if (var == "http_code") result += std::to_string(resp.status_code);
            else if (var == "size_download") result += std::to_string(size_download);
            else if (var == "size_upload") result += std::to_string(size_upload);
            else if (var == "time_total") {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.3f", resp.time_total);
                result += buf;
            }
            else if (var == "time_connect") {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.3f", resp.time_connect);
                result += buf;
            }
            else if (var == "url_effective") result += resp.final_url;
            else if (var == "content_type") {
                auto it = resp.headers.find("content-type");
                if (it != resp.headers.end()) result += it->second;
            }
            else if (var == "num_redirects") result += std::to_string(resp.num_redirects);
            else result += fmt.substr(i, end - i + 1); // unknown var, keep literal

            i = end; // advance past '}'
        } else {
            result += fmt[i];
        }
    }
    return result;
}

// ── Get remote filename from URL ────────────────────────────────────────────

static std::string get_remote_filename(const std::string& url) {
    // Find last / in path
    size_t last_slash = url.rfind('/');
    if (last_slash == std::string::npos || last_slash == url.size() - 1) {
        return "index.html";
    }
    std::string fname = url.substr(last_slash + 1);
    // Remove query string
    size_t q = fname.find('?');
    if (q != std::string::npos) fname = fname.substr(0, q);
    if (fname.empty()) fname = "index.html";
    return fname;
}

// ── Build custom headers vector from argtable ───────────────────────────────

static std::vector<std::pair<std::string, std::string>> build_headers(
        struct arg_str* header_opt, int header_count) {
    std::vector<std::pair<std::string, std::string>> headers;
    for (int i = 0; i < header_count; ++i) {
        if (header_opt[i].count > 0) {
            for (int j = 0; j < header_opt[i].count; ++j) {
                std::string h = header_opt[i].sval[j];
                size_t colon = h.find(':');
                if (colon != std::string::npos) {
                    std::string key = h.substr(0, colon);
                    std::string val = h.substr(colon + 1);
                    // Trim leading space from value
                    if (!val.empty() && val[0] == ' ') val = val.substr(1);
                    headers.emplace_back(key, val);
                } else {
                    headers.emplace_back(h, "");
                }
            }
        }
    }
    return headers;
}

// ── Command implementation ──────────────────────────────────────────────────

int curl_command(int argc, char** argv) {
    // ── Argument parsing ──
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* silent_opt = arg_lit0("s", "silent", "silent mode");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* include_opt = arg_lit0("i", "include", "include protocol headers in output");
    struct arg_lit* head_opt = arg_lit0("I", "head", "show headers only (HEAD request)");
    struct arg_lit* insecure_opt = arg_lit0("k", "insecure", "allow insecure server connections");
    struct arg_lit* follow_opt = arg_lit0("L", "location", "follow redirects");
    struct arg_lit* fail_opt = arg_lit0("f", "fail", "fail silently on HTTP errors (4xx/5xx)");
    struct arg_lit* fail_body_opt = arg_lit0(NULL, "fail-with-body", "fail with body on HTTP errors");
    struct arg_lit* get_opt = arg_lit0("G", "get", "append POST data to URL as query string");
    struct arg_lit* progress_bar_opt = arg_lit0(NULL, "progress-bar", "show progress as a bar");
    struct arg_lit* no_buffer_opt = arg_lit0(NULL, "no-buffer", "disable buffering");
    struct arg_lit* compressed_opt = arg_lit0(NULL, "compressed", "request compressed response");

    struct arg_str* request_opt = arg_str0("X", "request", "METHOD", "specify request method");
    struct arg_str* data_opt = arg_str0("d", "data", "DATA", "HTTP POST data");
    struct arg_str* data_raw_opt = arg_str0(NULL, "data-raw", "DATA", "HTTP POST data (raw)");
    struct arg_str* data_ascii_opt = arg_str0(NULL, "data-ascii", "DATA", "HTTP POST data (ASCII)");
    struct arg_str* data_binary_opt = arg_str0(NULL, "data-binary", "DATA", "HTTP POST data (binary)");
    struct arg_str* data_urlencode_opt = arg_str0(NULL, "data-urlencode", "DATA", "URL-encode POST data");
    struct arg_str* header_opt = arg_strn("H", "header", "HEADER", 0, 100, "custom header");
    struct arg_str* output_opt = arg_str0("o", "output", "FILE", "write to file");
    struct arg_lit* remote_name_opt = arg_lit0("O", "remote-name", "save as remote filename");
    struct arg_str* dump_header_opt = arg_str0("D", "dump-header", "FILE", "save headers to file");
    struct arg_str* user_opt = arg_str0("u", "user", "USER:PASSWORD", "basic authentication");
    struct arg_str* user_agent_opt = arg_str0("A", "user-agent", "AGENT", "set User-Agent");
    struct arg_str* referer_opt = arg_str0("e", "referer", "REFERER", "set Referer header");
    struct arg_str* cookie_opt = arg_str0("b", "cookie", "DATA", "send cookie");
    struct arg_str* writeout_opt = arg_str0("w", "write-out", "FORMAT", "output format after transfer");
    struct arg_dbl* max_time_opt = arg_dbl0("m", "max-time", "SECONDS", "maximum time");
    struct arg_dbl* conn_timeout_opt = arg_dbl0(NULL, "connect-timeout", "SECONDS", "connection timeout");
    struct arg_int* retry_opt = arg_int0(NULL, "retry", "NUM", "retry count on transient errors");
    struct arg_lit* retry_connrefused_opt = arg_lit0(NULL, "retry-connrefused", "retry on connection refused");
    struct arg_int* max_redirs_opt = arg_int0(NULL, "max-redirs", "NUM", "maximum redirects");

    struct arg_file* url_arg = arg_filen(NULL, NULL, "URL", 0, 1, "URL to fetch");
    struct arg_end* end = arg_end(20);

    std::vector<void*> table = {
        help_opt, version_opt, silent_opt, verbose_opt, include_opt, head_opt,
        insecure_opt, follow_opt, fail_opt, fail_body_opt, get_opt, progress_bar_opt,
        no_buffer_opt, compressed_opt,
        request_opt, data_opt, data_raw_opt, data_ascii_opt, data_binary_opt,
        data_urlencode_opt, header_opt, output_opt, remote_name_opt, dump_header_opt,
        user_opt, user_agent_opt, referer_opt, cookie_opt, writeout_opt,
        max_time_opt, conn_timeout_opt, retry_opt, retry_connrefused_opt,
        max_redirs_opt, url_arg, end
    };

    ArgTable argt(table);

    int nerrors = argt.parse(argc, argv);
    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // ── Help / Version ──
    if (help_opt->count > 0) {
        printf("Usage: %s [OPTIONS]... [URL]\n", argv[0]);
        printf("\nA Curl-like HTTP client for modbox.\n");
        printf("\nOptions:\n");
        printf("  -X, --request=METHOD    Specify request method\n");
        printf("  -d, --data=DATA         HTTP POST data\n");
        printf("      --data-urlencode=DATA  URL-encode POST data\n");
        printf("  -G, --get               Append POST data to URL as query string\n");
        printf("  -H, --header=HEADER     Custom header\n");
        printf("  -I, --head              Show headers only\n");
        printf("  -o, --output=FILE       Write to file\n");
        printf("  -O, --remote-name       Save as remote filename\n");
        printf("  -D, --dump-header=FILE  Save headers to file\n");
        printf("  -u, --user=USER:PASS    Basic authentication\n");
        printf("  -A, --user-agent=AGENT  Set User-Agent\n");
        printf("  -e, --referer=REFERER   Set Referer header\n");
        printf("  -b, --cookie=COOKIE     Send cookie\n");
        printf("  -s, --silent            Silent mode\n");
        printf("  -v, --verbose           Verbose output\n");
        printf("  -i, --include           Include headers in output\n");
        printf("  -k, --insecure          Allow insecure SSL connections\n");
        printf("  -L, --location          Follow redirects\n");
        printf("  -f, --fail              Fail on HTTP 4xx/5xx\n");
        printf("      --fail-with-body    Fail with body on HTTP errors\n");
        printf("  -m, --max-time=SECS     Maximum time\n");
        printf("      --connect-timeout=SECS  Connection timeout\n");
        printf("      --max-redirs=NUM    Maximum redirects (default 20)\n");
        printf("      --retry=NUM         Retry count on transient errors\n");
        printf("      --retry-connrefused Retry on connection refused\n");
        printf("  -w, --write-out=FORMAT  Output format after transfer\n");
        printf("      --progress-bar      Show progress as a bar\n");
        printf("      --no-buffer         Disable buffering\n");
        printf("      --compressed        Request compressed response\n");
        printf("  -h, --help              Display this help\n");
        printf("  -V, --version           Output version\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("curl");
        return 0;
    }

    // ── Validate ──
    if (url_arg->count == 0) {
        fprintf(stderr, "curl: no URL specified\n");
        return 1;
    }
    if (url_arg->count > 1) {
        fprintf(stderr, "curl: only one URL is supported\n");
        return 1;
    }

    // Conflicting flags
    if (output_opt->count > 0 && remote_name_opt->count > 0) {
        fprintf(stderr, "curl: --output and --remote-name conflict\n");
        return 1;
    }
    if (silent_opt->count > 0 && progress_bar_opt->count > 0) {
        fprintf(stderr, "curl: --silent and --progress-bar conflict\n");
        return 1;
    }
    if (head_opt->count > 0 && (data_opt->count > 0 || data_raw_opt->count > 0)) {
        fprintf(stderr, "curl: --head with --data is not supported\n");
        return 1;
    }

    // Out of scope warnings
    if (compressed_opt->count > 0) {
        fprintf(stderr, "curl: --compressed is not fully supported in this version\n");
    }

    // ── Build options ──
    CurlOptions opts;
    opts.url = url_arg->filename[0];

    // Method
    if (request_opt->count > 0) {
        opts.method = request_opt->sval[0];
    } else if (head_opt->count > 0) {
        opts.method = "HEAD";
    } else if (data_opt->count > 0 || data_raw_opt->count > 0 || data_ascii_opt->count > 0 ||
               data_binary_opt->count > 0 || data_urlencode_opt->count > 0) {
        opts.method = "POST";
    }

    // Data
    if (data_opt->count > 0) opts.post_data = data_opt->sval[0];
    else if (data_raw_opt->count > 0) opts.post_data = data_raw_opt->sval[0];
    else if (data_ascii_opt->count > 0) opts.post_data = data_ascii_opt->sval[0];
    else if (data_binary_opt->count > 0) opts.post_data = data_binary_opt->sval[0];
    else if (data_urlencode_opt->count > 0) {
        opts.post_data = data_urlencode_opt->sval[0];
        opts.data_urlencode = true;
    }

    if (opts.data_urlencode && !opts.post_data.empty()) {
        // Format is "key=value"
        size_t eq = opts.post_data.find('=');
        if (eq != std::string::npos) {
            opts.post_data = url_encode(opts.post_data.substr(0, eq)) + "=" +
                             url_encode(opts.post_data.substr(eq + 1));
        } else {
            opts.post_data = url_encode(opts.post_data);
        }
    }

    opts.get_with_data = (get_opt->count > 0);

    // If -G is used, force GET method (unless explicitly set by -X)
    if (opts.get_with_data && request_opt->count == 0) {
        opts.method = "GET";
    }

    // Headers
    opts.custom_headers = build_headers(header_opt, header_opt->count);

    // User-Agent
    if (user_agent_opt->count > 0) {
        opts.custom_headers.emplace_back("User-Agent", user_agent_opt->sval[0]);
    }

    // Referer
    if (referer_opt->count > 0) {
        opts.custom_headers.emplace_back("Referer", referer_opt->sval[0]);
    }

    // Cookie
    if (cookie_opt->count > 0) {
        opts.custom_headers.emplace_back("Cookie", cookie_opt->sval[0]);
    }

    // Output
    if (output_opt->count > 0) {
        opts.output_file = output_opt->sval[0];
    } else if (remote_name_opt->count > 0) {
        opts.remote_name = get_remote_filename(opts.url);
    }

    // Dump header
    if (dump_header_opt->count > 0) {
        opts.dump_header_file = dump_header_opt->sval[0];
    }

    // Auth
    if (user_opt->count > 0) {
        std::string auth = user_opt->sval[0];
        size_t colon = auth.find(':');
        if (colon != std::string::npos) {
            opts.user = auth.substr(0, colon);
            opts.password = auth.substr(colon + 1);
        } else {
            opts.user = auth;
        }
        // Add Authorization header
        std::string creds = opts.user + ":" + opts.password;
        // Base64 encode
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string b64str;
        for (size_t i = 0; i < creds.size(); i += 3) {
            unsigned char a = creds[i], b = (i + 1 < creds.size()) ? creds[i + 1] : 0,
                          c = (i + 2 < creds.size()) ? creds[i + 2] : 0;
            b64str += b64[a >> 2];
            b64str += b64[((a & 0x3) << 4) | (b >> 4)];
            b64str += (i + 1 < creds.size()) ? b64[((b & 0xF) << 2) | (c >> 6)] : '=';
            b64str += (i + 2 < creds.size()) ? b64[c & 0x3F] : '=';
        }
        opts.custom_headers.emplace_back("Authorization", "Basic " + b64str);
    }

    // Behavior
    opts.silent = (silent_opt->count > 0);
    opts.verbose = (verbose_opt->count > 0);
    opts.include_headers = (include_opt->count > 0);
    opts.head_only = (head_opt->count > 0);
    opts.insecure = (insecure_opt->count > 0);
    opts.follow_redirects = (follow_opt->count > 0);
    opts.fail_on_http_error = (fail_opt->count > 0);
    opts.fail_with_body = (fail_body_opt->count > 0);
    opts.show_writeout = (writeout_opt->count > 0);
    opts.writeout_format = writeout_opt->count > 0 ? writeout_opt->sval[0] : "";
    opts.show_progress = (!opts.silent);
    opts.progress_bar = (progress_bar_opt->count > 0);
    opts.max_time = (max_time_opt->count > 0) ? max_time_opt->dval[0] : 0.0;
    opts.connect_timeout = (conn_timeout_opt->count > 0) ? conn_timeout_opt->dval[0] : 0.0;
    opts.retry_count = (retry_opt->count > 0) ? retry_opt->ival[0] : 0;
    opts.retry_connrefused = (retry_connrefused_opt->count > 0);
    opts.max_redirs = (max_redirs_opt->count > 0) ? max_redirs_opt->ival[0] : 20;

    // ── Execute ──
    HttpResponse response;
    int rc = http_request(opts, response);
    if (rc != 0) return rc;

    // ── Writeout ──
    if (opts.show_writeout) {
        long size_down = (long)response.body.size();
        std::string out = format_writeout(opts.writeout_format, response, size_down, 0);
        printf("%s", out.c_str());
        return 0;
    }

    // ── Check for HTTP error with -f ──
    if (opts.fail_on_http_error && response.status_code >= 400) {
        if (opts.fail_with_body && !response.body.empty()) {
            // Print body to stdout
            if (opts.output_file.empty() && opts.dump_header_file.empty()) {
                printf("%s", response.body.c_str());
            }
        }
        fprintf(stderr, "curl: (22) The requested URL returned error: %d\n", response.status_code);
        return 22;
    }

    // ── Output body ──
    long size_down = (long)response.body.size();

    // Progress
    if (!opts.silent && isatty(STDERR_FILENO)) {
        print_progress(size_down, size_down, 0, 0, response.time_total, response.time_total,
                       opts.silent, opts.progress_bar);
        clear_progress();
        fprintf(stderr, "\n");
    }

    // Dump headers to file
    if (!opts.dump_header_file.empty()) {
        FILE* f = fopen(opts.dump_header_file.c_str(), "w");
        if (!f) {
            fprintf(stderr, "curl: Failed to open file '%s': %s\n",
                    opts.dump_header_file.c_str(), strerror(errno));
            return 1;
        }
        fprintf(f, "HTTP/1.1 %d %s\r\n", response.status_code, response.status_text.c_str());
        for (auto& h : response.headers) {
            fprintf(f, "%s: %s\r\n", h.first.c_str(), h.second.c_str());
        }
        fprintf(f, "\r\n");
        fclose(f);
    }

    // Include headers in output
    if ((opts.include_headers || opts.head_only) && opts.output_file.empty() && opts.dump_header_file.empty()) {
        printf("HTTP/1.1 %d %s\r\n", response.status_code, response.status_text.c_str());
        for (auto& h : response.headers) {
            printf("%s: %s\r\n", h.first.c_str(), h.second.c_str());
        }
        printf("\r\n");
    }

    // Write to file or stdout
    if (!opts.output_file.empty()) {
        FILE* f = fopen(opts.output_file.c_str(), "w");
        if (!f) {
            fprintf(stderr, "curl: Failed to open file '%s': %s\n",
                    opts.output_file.c_str(), strerror(errno));
            return 1;
        }
        fwrite(response.body.c_str(), 1, response.body.size(), f);
        fclose(f);
    } else if (!opts.remote_name.empty()) {
        FILE* f = fopen(opts.remote_name.c_str(), "w");
        if (!f) {
            fprintf(stderr, "curl: Failed to open file '%s': %s\n",
                    opts.remote_name.c_str(), strerror(errno));
            return 1;
        }
        fwrite(response.body.c_str(), 1, response.body.size(), f);
        fclose(f);
    } else {
        printf("%s", response.body.c_str());
    }

    return 0;
}

REGISTER_COMMAND("curl", curl_command, "Make HTTP requests");
