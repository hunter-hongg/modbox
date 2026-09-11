#include "commands/wget.hpp"

#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "commands/http_client.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/arg_util.hpp"

static const char* HELP_TEXT = R"(Usage: wget [OPTION]... URL

Download files from HTTP/HTTPS servers.

Output control:
  -O, --output-document=FILE   Write to FILE
  -o, --output-file=FILE       Write log to FILE (conflicts with -O)
  -P, --directory-prefix=PREFIX  Save to PREFIX/
  -nc, --no-clobber            Skip download if file exists
  -c, --continue               Resume partially downloaded file
  -nH, --no-host-directories   Omit host directory in -P
  -x, --force-directories      Create full directory structure

Display control:
  -q, --quiet                  Quiet mode (no progress)
  -v, --verbose                Verbose output
  --spider                     Don't download, check existence only
  --progress=TYPE              Progress meter: bar (default), dot, none
  -S, --server-response        Print server response headers

Network options:
  -t, --tries=NUMBER           Set number of retries (default 3)
  --timeout=SECS               Set timeout value
  --tries=NUMBER               Alias for --tries
  -4, --inet4-only             Force IPv4
  -6, --inet6-only             Force IPv6
  --no-check-certificate       Don't validate server certificates
  --limit-rate=BYTES           Throttle download rate

Authentication:
  --auth-user=USER             HTTP auth username
  --password=PASS              HTTP auth password

Request options:
  -U, --user-agent=STRING      Set User-Agent
  --referer=URL                Set Referer header
  --header=HEADER              Extra header
  -X, --post-data=STRING       Send POST data
  --content-disposition        Use Content-Disposition filename

Miscellaneous:
  -b, --background             Go to background after parsing options
      --help                   Display this help
      --version                Output version information
)";

static void print_help(const char* prog) {
    printf("wget (modbox) - download a file from a URL\n\n");
    printf("Usage: %s [OPTION]... URL\n\n", prog);
    printf("%s\n", HELP_TEXT);
}

// Pre-process argv: expand combined short flags like -nc, -nH into long options.
static std::vector<char*> preprocess_argv(int argc, char** argv) {
    std::vector<char*> out;
    out.reserve(argc * 2);
    for (int i = 0; i < argc; ++i) {
        std::string const arg(argv[i]);
        if (arg == "-nc") {
            out.push_back(strdup("--no-clobber"));
        } else if (arg == "-nH") {
            out.push_back(strdup("--no-host-directories"));
        } else {
            out.push_back(argv[i]);
        }
    }
    return out;
}

// Recursively create directories
static bool mkdirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            struct stat st{};
            if (stat(cur.c_str(), &st) != 0) {
                if (mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) {
                    return false;
                }
            }
        }
    }
    return true;
}

// Get remote filename from Content-Disposition header
static std::string get_content_disp_filename(const std::string& cd_value) {
    // Try filename*=UTF-8''encoded_name
    size_t pos = cd_value.find("filename*=");
    if (pos != std::string::npos) {
        pos += 10;
        // Skip encoding prefix like "UTF-8''"
        size_t const quote1 = cd_value.find('\'', pos);
        if (quote1 != std::string::npos) {
            size_t const quote2 = cd_value.find('\'', quote1 + 1);
            if (quote2 != std::string::npos) {
                std::string encoded = cd_value.substr(quote1 + 1, quote2 - quote1 - 1);
                // Simple URL decode
                std::string result;
                for (size_t i = 0; i < encoded.size(); ++i) {
                    if (encoded[i] == '%' && i + 2 < encoded.size()) {
                        std::string const hex = encoded.substr(i + 1, 2);
                        char* end;
                        int const val = static_cast<int>(strtol(hex.c_str(), &end, 16));
                        if (end == hex.c_str() + 2) {
                            result += static_cast<char>(val);
                            i += 2;
                        } else {
                            result += encoded[i];
                        }
                    } else if (encoded[i] == '+') {
                        result += ' ';
                    } else {
                        result += encoded[i];
                    }
                }
                return result;
            }
        }
    }
    // Try filename="value" or filename=value
    pos = cd_value.find("filename=");
    if (pos != std::string::npos) {
        pos += 9;
        std::string fname;
        // Skip whitespace
        while (pos < cd_value.size() && cd_value[pos] == ' ') { ++pos;
}
        if (pos < cd_value.size() && cd_value[pos] == '"') {
            ++pos;
            size_t const end = cd_value.find('"', pos);
            if (end != std::string::npos) {
                fname = cd_value.substr(pos, end - pos);
            }
        } else {
            size_t const end = cd_value.find_first_of("; ", pos);
            if (end != std::string::npos) {
                fname = cd_value.substr(pos, end - pos);
            }
        }
        return fname;
    }
    return "";
}

// Get remote filename from URL path
static std::string get_remote_filename(const std::string& url) {
    // Find the path portion after the host
    size_t pos = url.find("://");
    if (pos == std::string::npos) { pos = 0;
    } else { pos += 3;
}

    // Skip past host
    size_t const host_end = url.find('/', pos);
    if (host_end == std::string::npos) { return "index.html";
}

    std::string path = url.substr(host_end);
    // Remove query string
    size_t const q = path.find('?');
    if (q != std::string::npos) { path = path.substr(0, q);
}
    if (path.empty() || path == "/") { return "index.html";
}

    // Get basename
    size_t const last_slash = path.rfind('/');
    if (last_slash == std::string::npos) { return path;
}
    std::string fname = path.substr(last_slash + 1);
    if (fname.empty()) { return "index.html";
}
    return fname;
}

int wget_command(int argc, char** argv) {
    // ── Pre-process combined short flags ──
    std::vector<char*> pre_argc;
    pre_argc = preprocess_argv(argc, argv);
    int const pargc = static_cast<int>(pre_argc.size());
    char** pargv = pre_argc.data();

    // ── Argtable3 for all flags ──
    struct arg_lit* help_opt       = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt    = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* quiet_opt      = arg_lit0("q", "quiet", "quiet mode (no progress)");
    struct arg_lit* verbose_opt    = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* server_resp_opt = arg_lit0("S", "server-response", "print server response");
    struct arg_lit* spider_opt     = arg_lit0(NULL, "spider", "check URL without downloading");
    struct arg_lit* no_clobber_opt = arg_lit0(NULL, "no-clobber", "skip download if file exists");
    struct arg_lit* continue_dl_opt = arg_lit0("c", "continue", "resume download");
    struct arg_lit* background_opt = arg_lit0("b", "background", "go to background");
    struct arg_lit* insecure_opt   = arg_lit0(NULL, "no-check-certificate", "disable cert check");
    struct arg_lit* content_disp_opt = arg_lit0(NULL, "content-disposition", "use Content-Disposition filename");
    struct arg_lit* no_host_dir_opt = arg_lit0(NULL, "no-host-directories", "omit host dir in -P");
    struct arg_lit* use_full_dir_opt = arg_lit0("x", "force-directories", "create full directory structure");

    struct arg_str* output_doc_opt   = arg_str0("O", "output-document", "FILE", "write to FILE");
    struct arg_str* output_file_opt  = arg_str0("o", "output-file", "FILE", "write log to FILE");
    struct arg_str* dir_prefix_opt   = arg_str0("P", "directory-prefix", "PREFIX", "save to PREFIX/");
    struct arg_str* user_agent_opt   = arg_str0("U", "user-agent", "STRING", "set User-Agent");
    struct arg_str* referer_opt      = arg_str0(NULL, "referer", "URL", "set Referer header");
    struct arg_str* auth_user_opt    = arg_str0(NULL, "auth-user", "USER", "HTTP auth username");
    struct arg_str* auth_pass_opt    = arg_str0(NULL, "password", "PASS", "HTTP auth password");
    struct arg_str* post_data_opt    = arg_str0("X", "post-data", "STRING", "send POST data");
    struct arg_str* progress_opt     = arg_str0(NULL, "progress", "TYPE", "progress meter: bar/dot/none");
    struct arg_dbl* timeout_opt      = arg_dbl0(NULL, "timeout", "SECS", "set timeout");
    struct arg_int* tries_opt        = arg_int0("t", "tries", "NUMBER", "number of retries");
    struct arg_lit* inet4_opt        = arg_lit0("4", "inet4-only", "force IPv4");
    struct arg_lit* inet6_opt        = arg_lit0("6", "inet6-only", "force IPv6");
    struct arg_str* limit_rate_opt   = arg_str0(NULL, "limit-rate", "BYTES", "throttle rate");
    struct arg_str* header_opt       = arg_strn("H", "header", "HEADER", 0, 100, "extra header");

    struct arg_str* pos_arg          = arg_strn(NULL, NULL, "ARG", 0, 1, "positional arg");
    struct arg_end* end              = arg_end(20);

    std::vector<void*> const table = {
        help_opt, version_opt, quiet_opt, verbose_opt, server_resp_opt, spider_opt,
        no_clobber_opt, continue_dl_opt, background_opt, insecure_opt, content_disp_opt,
        no_host_dir_opt, use_full_dir_opt,
        output_doc_opt, output_file_opt, dir_prefix_opt,
        user_agent_opt, referer_opt, auth_user_opt, auth_pass_opt, post_data_opt,
        progress_opt, timeout_opt, tries_opt, inet4_opt, inet6_opt, limit_rate_opt,
        header_opt, pos_arg, end
    };

    ArgTable argt(table);

    int const nerrors = argt.parse(pargc, pargv);
    if (nerrors > 0) {
        arg_print_errors(stderr, end, pargv[0]);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", pargv[0]);
        return 1;
    }

    // ── Help / Version ──
    if (help_opt->count > 0) {
        print_help(pargv[0]);
        return 0;
    }
    if (version_opt->count > 0) {
        print_version("wget");
        return 0;
    }

    // ── Collect positional args ──
    std::string url;
    for (int i = 0; i < pos_arg->count; ++i) {
        url = pos_arg->sval[i];
    }

    // ── Validate positional args ──
    if (url.empty()) {
        (void)fprintf(stderr, "wget: missing URL\n");
        return 1;
    }

    // ── Validate conflicts ──
    if (output_doc_opt->count > 0 && output_file_opt->count > 0) {
        (void)fprintf(stderr, "wget: --output-document and --output-file conflict\n");
        return 1;
    }
    if (no_clobber_opt->count > 0 && continue_dl_opt->count > 0) {
        (void)fprintf(stderr, "wget: --no-clobber and --continue conflict\n");
        return 1;
    }
    if (background_opt->count > 0 && output_file_opt->count > 0) {
        (void)fprintf(stderr, "wget: --background and --output-file conflict\n");
        return 1;
    }

    // ── Build CurlOptions ──
    CurlOptions opts;
    opts.url = url;
    opts.follow_redirects = true;
    opts.retry_count = tries_opt->count > 0 ? tries_opt->ival[0] : 0;
    opts.max_time = timeout_opt->count > 0 ? timeout_opt->dval[0] : 0.0;
    opts.insecure = insecure_opt->count > 0;
    opts.silent = quiet_opt->count > 0;
    opts.verbose = verbose_opt->count > 0;

    // Progress type
    if (progress_opt->count > 0) {
        std::string const prog_type = progress_opt->sval[0];
        if (prog_type == "dot") {
            opts.show_progress = true;
            opts.progress_bar = false;
        } else if (prog_type == "none") {
            opts.show_progress = false;
        } else {
            // "bar" or default
            opts.show_progress = true;
            opts.progress_bar = true;
        }
    } else {
        opts.show_progress = quiet_opt->count == 0;
        opts.progress_bar = true;
    }

    // Headers
    for (int i = 0; i < header_opt->count; ++i) {
        std::string const h = header_opt->sval[i];
        size_t const colon = h.find(':');
        if (colon != std::string::npos) {
            std::string const key = h.substr(0, colon);
            std::string val = h.substr(colon + 1);
            if (!val.empty() && val[0] == ' ') { val = val.substr(1);
}
            opts.custom_headers.emplace_back(key, val);
        } else {
            opts.custom_headers.emplace_back(h, "");
        }
    }

    // User agent, referer, auth
    if (user_agent_opt->count > 0) {
        opts.custom_headers.emplace_back("User-Agent", user_agent_opt->sval[0]);
    }
    if (referer_opt->count > 0) {
        opts.custom_headers.emplace_back("Referer", referer_opt->sval[0]);
    }
    if (auth_user_opt->count > 0) {
        opts.user = auth_user_opt->sval[0];
    }
    if (auth_pass_opt->count > 0) {
        opts.password = auth_pass_opt->sval[0];
    }

    // POST data
    if (post_data_opt->count > 0) {
        opts.method = "POST";
        opts.post_data = post_data_opt->sval[0];
    }

    // Spider mode: HEAD request
    if (spider_opt->count > 0) {
        opts.method = "HEAD";
    }

    // ── Determine output file ──
    std::string output_file;
    bool output_to_stdout = false;

    if (output_doc_opt->count > 0) {
        output_file = output_doc_opt->sval[0];
        if (output_file == "-") {
            output_to_stdout = true;
            output_file.clear();
        }
    } else if (output_file_opt->count > 0) {
        // Log file (we still download to default but write log)
        (void)output_file_opt->sval[0]; // log file not fully supported in v1
    }

    // ── Resolve output path ──
    if (output_file.empty()) {
        std::string const remote_fname = get_remote_filename(url);
        std::string const disp_fname;
        if (content_disp_opt->count > 0) {
            // We'll check Content-Disposition after the request
        }

        if (dir_prefix_opt->count > 0) {
            std::string prefix = dir_prefix_opt->sval[0];
            if ((no_host_dir_opt->count == 0) && use_full_dir_opt->count == 0) {
                // Parse host from URL
                UrlParts up;
                if (parse_url(url.c_str(), up)) {
                    prefix += "/" + up.host;
                }
            }
            if (use_full_dir_opt->count > 0) {
                // Create full directory structure: prefix/host/path
                UrlParts up;
                if (parse_url(url.c_str(), up)) {
                    prefix += up.path;
                    // Remove trailing slash
                    if (!prefix.empty() && prefix.back() == '/') { prefix.pop_back();
}
                    // Find last slash for directory
                    size_t const last_slash = prefix.rfind('/');
                    if (last_slash != std::string::npos) {
                        std::string const dir = prefix.substr(0, last_slash + 1);
                        mkdirs(dir);
                    }
                    output_file = prefix;
                } else {
                    output_file = prefix + "/" + remote_fname;
                }
            } else {
                output_file = prefix + "/" + remote_fname;
            }
        } else {
            output_file = remote_fname;
        }
    }

    // ── No-clobber check ──
    if (no_clobber_opt->count > 0 && !output_to_stdout) {
        struct stat st{};
        if (stat(output_file.c_str(), &st) == 0) {
            (void)fprintf(stderr, "wget: File '%s' already there. Not retrieving.\n", output_file.c_str());
            return 0;
        }
    }

    // ── Spider mode ──
    if (spider_opt->count > 0) {
        HttpResponse resp;
        int const rc = http_request(opts, resp);
        if (rc != 0) {
            return 1;
        }
        if (resp.status_code >= 200 && resp.status_code < 400) {
            return 0;
        }
        (void)fprintf(stderr, "wget: server response: %d %s\n", resp.status_code, resp.status_text.c_str());
        return 1;
    }

    // ── Prepare for potential resume ──
    long resume_from = 0;
    if (continue_dl_opt->count > 0 && !output_to_stdout) {
        struct stat st{};
        if (stat(output_file.c_str(), &st) == 0 && st.st_size > 0) {
            resume_from = static_cast<long>(st.st_size);
        }
    }

    // ── Make the request ──
    // For resume, we need Range header
    if (resume_from > 0) {
        std::string const range_header = "Range: bytes=" + std::to_string(resume_from) + "-";
        opts.custom_headers.emplace_back(range_header.substr(0, range_header.find('=')) == "Range" ? "Range" : "Range",
            range_header.substr(range_header.find('=') + 1));
        // Actually just add as a proper header
        opts.custom_headers.pop_back(); // remove the one we just added
        opts.custom_headers.emplace_back("Range", "bytes=" + std::to_string(resume_from) + "-");
    }

    HttpResponse resp;
    int rc = http_request(opts, resp);
    if (rc != 0) {
        return 1;
    }

    // ── Handle redirect response in spider mode ──
    if (spider_opt->count > 0) {
        if (resp.status_code >= 200 && resp.status_code < 400) {
            return 0;
        }
        (void)fprintf(stderr, "wget: server response: %d %s\n", resp.status_code, resp.status_text.c_str());
        return 1;
    }

    // ── Handle Content-Disposition if requested ──
    if (content_disp_opt->count > 0 && !output_to_stdout) {
        auto it = resp.headers.find("content-disposition");
        if (it != resp.headers.end()) {
            std::string const disp_fname = get_content_disp_filename(it->second);
            if (!disp_fname.empty()) {
                output_file = disp_fname;
            }
        }
    }

    // ── Write output ──
    if (output_to_stdout) {
        (void)fwrite(resp.body.data(), 1, resp.body.size(), stdout);
        (void)fflush(stdout);
    } else {
        // Create parent directory if needed
        std::string const dir = output_file.substr(0, output_file.rfind('/'));
        if (!dir.empty()) {
            mkdirs(dir);
        }

        // Open file for writing
        int fd;
        if (resume_from > 0) {
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        } else {
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (fd < 0) {
            (void)fprintf(stderr, "wget: Failed to open file '%s': %s\n", output_file.c_str(), strerror(errno));
            return 1;
        }

        // Check for partial content on resume
        if (resume_from > 0 && resp.status_code != 206) {
            // Server doesn't support range requests, close and redo
            close(fd);
            if (remove(output_file.c_str()) != 0 && errno != ENOENT) {
                // Try to continue anyway
            }
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                (void)fprintf(stderr, "wget: Failed to open file '%s': %s\n", output_file.c_str(), strerror(errno));
                return 1;
            }
            // Re-fetch without range
            HttpResponse resp2;
            opts.custom_headers.erase(
                std::remove_if(opts.custom_headers.begin(), opts.custom_headers.end(),
                    [](const std::pair<std::string, std::string>& p) { return p.first == "Range"; }),
                opts.custom_headers.end());
            rc = http_request(opts, resp2);
            if (rc != 0) {
                close(fd);
                return 1;
            }
            resp = resp2;
        }

        ssize_t const written = write(fd, resp.body.data(), resp.body.size());
        close(fd);
        if (static_cast<size_t>(written) != resp.body.size()) {
            (void)fprintf(stderr, "wget: Failed to write to file '%s': %s\n", output_file.c_str(), strerror(errno));
            return 1;
        }
    }

    // ── Success message ──
    if (quiet_opt->count == 0) {
        (void)fprintf(stderr, "wget: saved [%ld] %s\n", static_cast<long>(resp.body.size()), output_file.c_str());
    }

    // ── HTTP error check ──
    if (resp.status_code >= 400) {
        (void)fprintf(stderr, "wget: HTTP error %d %s\n", resp.status_code, resp.status_text.c_str());
        return 1;
    }

    return 0;
}

REGISTER_COMMAND("wget", wget_command, "Download a file from a URL");
