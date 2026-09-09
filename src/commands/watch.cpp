#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <poll.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "commands/watch.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

constexpr double kDefaultInterval = 2.0;
constexpr int kFallbackColumns = 80;
constexpr int kExitUsage = 2;
constexpr int kExitCommandFailed = 127;

struct WATCHOptions {
    bool no_title = false;
    bool beep = false;
    bool errexit = false;
    bool chgexit = false;
    bool differences = false;
    bool exec_mode = false;
    double interval = kDefaultInterval;
    bool is_tty = false;
};

void print_help(const char* prog) {
    printf("Usage: %s [options] command [arg...]\n", prog);
    printf("Execute a program periodically, showing output fullscreen.\n");
    printf("\n");
    printf("  -b, --beep             beep if command has a non-zero exit\n");
    printf("  -d, --differences[=permanent]\n");
    printf("                         highlight changes between updates\n");
    printf("  -e, --errexit          exit if command has a non-zero exit\n");
    printf("  -g, --chgexit          exit when output from command changes\n");
    printf("  -n, --interval <secs>  seconds to wait between updates\n");
    printf("                         (default 2.0; accepts s/m/h/d suffix)\n");
    printf("  -t, --no-title         turn off header\n");
    printf("  -x, --exec             pass command to exec instead of \"sh -c\"\n");
    printf("  -h, --help             display this help and exit\n");
    printf("  -v, --version          output version information and exit\n");
}

void usage_hint(const char* prog) {
    (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
}

bool parse_interval(const char* s, double* out) {
    if (s == nullptr || *s == '\0') { return false;
}
    char* endp = nullptr;
    errno = 0;
    double const val = strtod(s, &endp);
    if (endp == s || val <= 0.0) { return false;
}
    double mult = 1.0;
    if (*endp != '\0') {
        switch (*endp) {
            case 's': mult = 1.0; break;
            case 'm': mult = 60.0; break;
            case 'h': mult = 3600.0; break;
            case 'd': mult = 86400.0; break;
            default: return false;
        }
        endp++;
        if (*endp != '\0') { return false;
}
    }
    double const total = val * mult;
    if (!std::isfinite(total) || total <= 0.0 || total > 1e6) { return false;
}
    *out = total;
    return true;
}

std::string join_command(int cmd_start, char** argv) {
    std::string cmdline;
    for (int i = cmd_start; argv[i] != nullptr; i++) {
        if (!cmdline.empty()) { cmdline += ' ';
}
        cmdline += argv[i];
    }
    return cmdline;
}

// Run the command once, capturing its stdout. Child stderr is inherited.
// The parent drains the pipe concurrently with poll() while reaping the
// child with waitpid(WNOHANG), so frames larger than the pipe buffer do
// not deadlock. Returns false on fork/pipe failure.
bool run_frame(int cmd_start, char** argv, const WATCHOptions* opts,
               std::string* out, int* exit_code) {
    int fds[2];
    if (pipe(fds) != 0) {
        (void)fprintf(stderr, "watch: pipe failed: %s\n", strerror(errno));
        return false;
    }
    pid_t const pid = fork();
    if (pid < 0) {
        (void)fprintf(stderr, "watch: fork failed: %s\n", strerror(errno));
        (void)close(fds[0]);
        (void)close(fds[1]);
        return false;
    }
    if (pid == 0) {
        (void)close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0) { _exit(kExitCommandFailed);
}
        (void)close(fds[1]);
        if (opts->exec_mode) {
            execvp(argv[cmd_start], &argv[cmd_start]);
            (void)fprintf(stderr, "watch: failed to run '%s': %s\n",
                          argv[cmd_start], strerror(errno));
        } else {
            std::string const cmdline = join_command(cmd_start, argv);
            execl("/bin/sh", "sh", "-c", cmdline.c_str(), (char*)nullptr);
            (void)fprintf(stderr, "watch: failed to run '/bin/sh': %s\n",
                          strerror(errno));
        }
        _exit(kExitCommandFailed);
    }
    (void)close(fds[1]);
    out->clear();
    char buf[4096];
    int status = 0;
    bool child_reaped = false;
    bool eof = false;
    while (!(child_reaped && eof)) {
        struct pollfd pfd;
        pfd.fd = fds[0];
        pfd.events = POLLIN;
        int const pr = poll(&pfd, 1, 200);
        if (pr < 0) {
            if (errno == EINTR) { continue;
}
            break;
        }
        if (pr > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            ssize_t const n = read(fds[0], buf, sizeof buf);
            if (n > 0) {
                out->append(buf, static_cast<size_t>(n));
            } else if (n == 0) {
                eof = true;
            } else if (errno != EINTR) {
                eof = true;
            }
        }
        if (!child_reaped) {
            pid_t const r = waitpid(pid, &status, WNOHANG);
            if (r == pid) { child_reaped = true;
}
        }
    }
    (void)close(fds[0]);
    if (!child_reaped) {
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    }
    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        *exit_code = 128 + WTERMSIG(status);
    } else {
        *exit_code = 1;
    }
    return true;
}

std::string title_right() {
    char hostname[256] = "";
    if (gethostname(hostname, sizeof hostname - 1) != 0) { hostname[0] = '\0';
}
    time_t const t = time(nullptr);
    std::string date = t != (time_t)-1 ? ctime(&t) : "";
    if (!date.empty() && date.back() == '\n') { date.pop_back();
}
    std::string right = hostname;
    right += ", ";
    right += date;
    return right;
}

int terminal_columns(const WATCHOptions* opts) {
    if (!opts->is_tty) { return kFallbackColumns;
}
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return kFallbackColumns;
}

void print_title(double interval, const std::string& command_line,
                 const WATCHOptions* opts) {
    char iv[64];
    (void)snprintf(iv, sizeof iv, "%.1f", interval);
    std::string left = "Every ";
    left += iv;
    left += "s: ";
    left += command_line;
    std::string const right = title_right();
    int const width = terminal_columns(opts);
    long const pad =
        static_cast<long>(width) - static_cast<long>(left.size()) -
        static_cast<long>(right.size());
    if (pad > 1) { left.append(static_cast<size_t>(pad), ' ');
}
    left += right;
    (void)fwrite(left.data(), 1, left.size(), stdout);
    (void)fputc('\n', stdout);
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t const nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

void print_frame(const std::string& output, const std::string* prev_output,
                 const WATCHOptions* opts) {
    if (!opts->differences || !opts->is_tty || prev_output == nullptr) {
        (void)fwrite(output.data(), 1, output.size(), stdout);
        return;
    }
    std::vector<std::string> const prev_lines = split_lines(*prev_output);
    std::vector<std::string> const cur_lines = split_lines(output);
    size_t const n = prev_lines.size() > cur_lines.size() ? prev_lines.size()
                                                          : cur_lines.size();
    for (size_t i = 0; i < n; i++) {
        std::string const cur = i < cur_lines.size() ? cur_lines[i] : "";
        std::string const prev = i < prev_lines.size() ? prev_lines[i] : "";
        bool const changed = cur != prev;
        if (changed) { (void)fputs("\033[7m", stdout);
}
        (void)fwrite(cur.data(), 1, cur.size(), stdout);
        if (changed) { (void)fputs("\033[0m", stdout);
}
        (void)fputc('\n', stdout);
    }
}

int watch_command(int argc, char** argv) {
    const char* prog = argc > 0 ? argv[0] : "watch";
    WATCHOptions opts;
    opts.is_tty = isatty(STDOUT_FILENO) != 0;
    int cmd_start = -1;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--") == 0) {
            cmd_start = i + 1;
            break;
        }
        if (a[0] != '-' || a[1] == '\0') {
            cmd_start = i;
            break;
        }
        if (a[1] == '-') {
            const char* lopt = a + 2;
            if (strcmp(lopt, "help") == 0) {
                print_help(prog);
                return 0;
            }
            if (strcmp(lopt, "version") == 0) {
                print_version("watch");
                return 0;
            }
            if (strcmp(lopt, "beep") == 0) { opts.beep = true; continue; }
            if (strcmp(lopt, "errexit") == 0) { opts.errexit = true; continue; }
            if (strcmp(lopt, "chgexit") == 0) { opts.chgexit = true; continue; }
            if (strcmp(lopt, "no-title") == 0) { opts.no_title = true; continue; }
            if (strcmp(lopt, "exec") == 0) { opts.exec_mode = true; continue; }
            if (strcmp(lopt, "differences") == 0) {
                opts.differences = true;
                continue;
            }
            const char* val = nullptr;
            if (strncmp(lopt, "differences=", 12) == 0) {
                val = lopt + 12;
                if (strcmp(val, "permanent") != 0) {
                    (void)fprintf(stderr,
                                  "%s: invalid argument '%s' for '--differences'\n",
                                  prog, val);
                    usage_hint(prog);
                    return kExitUsage;
                }
                opts.differences = true;
                continue;
            }
            if (strncmp(lopt, "interval=", 9) == 0) {
                if (!parse_interval(lopt + 9, &opts.interval)) {
                    (void)fprintf(stderr, "%s: invalid interval '%s'\n", prog,
                                  lopt + 9);
                    usage_hint(prog);
                    return kExitUsage;
                }
                continue;
            }
            (void)fprintf(stderr, "%s: unrecognized option '--%s'\n", prog, lopt);
            usage_hint(prog);
            return kExitUsage;
        }
        bool need_val = false;
        const char* glued = nullptr;
        for (const char* p = a + 1; *p != '\0' || need_val; p++) {
            if (need_val) {
                const char* val = glued != nullptr ? glued : argv[i + 1];
                if (val == nullptr) {
                    (void)fprintf(stderr,
                                  "%s: option '-n' requires an argument\n", prog);
                    usage_hint(prog);
                    return kExitUsage;
                }
                if (!parse_interval(val, &opts.interval)) {
                    (void)fprintf(stderr, "%s: invalid interval '%s'\n", prog,
                                  val);
                    usage_hint(prog);
                    return kExitUsage;
                }
                if (glued == nullptr) { i++;
}
                need_val = false;
                glued = nullptr;
                break;
            }
            switch (*p) {
                case 'b': opts.beep = true; break;
                case 'e': opts.errexit = true; break;
                case 'g': opts.chgexit = true; break;
                case 't': opts.no_title = true; break;
                case 'x': opts.exec_mode = true; break;
                case 'd': {
                    opts.differences = true;
                    if (p[1] != '\0') {
                        const char* arg = p + 1;
                        if (strcmp(arg, "permanent") != 0) {
                            (void)fprintf(stderr,
                                          "%s: invalid argument '%s' for '-d'\n",
                                          prog, arg);
                            usage_hint(prog);
                            return kExitUsage;
                        }
                        p += strlen(arg);
                    }
                    break;
                }
                case 'n': {
                    need_val = true;
                    if (p[1] != '\0') {
                        glued = p + 1;
                        p += strlen(p) - 1;
                    }
                    break;
                }
                case 'h':
                    print_help(prog);
                    return 0;
                case 'v':
                    print_version("watch");
                    return 0;
                default:
                    (void)fprintf(stderr, "%s: invalid option -- '%c'\n", prog, *p);
                    usage_hint(prog);
                    return kExitUsage;
            }
            if (*p == '\0') { break;
}
        }
    }

    if (cmd_start < 0 || cmd_start >= argc || argv[cmd_start] == nullptr) {
        (void)fprintf(stderr, "%s: missing command\n", prog);
        usage_hint(prog);
        return kExitUsage;
    }

    std::string const command_line = join_command(cmd_start, argv);
    std::string output;
    std::string prev_output;
    bool have_prev = false;

    while (true) {
        int exit_code = 0;
        if (!run_frame(cmd_start, argv, &opts, &output, &exit_code)) {
            return 1;
        }
        if (opts.is_tty) { (void)fputs("\033[H\033[2J", stdout);
}
        if (!opts.no_title) {
            print_title(opts.interval, command_line, &opts);
        }
        print_frame(output, have_prev ? &prev_output : nullptr, &opts);
        (void)fflush(stdout);
        if (opts.beep && exit_code != 0) { (void)fputc('\007', stdout);
        }
        (void)fflush(stdout);
        if (opts.errexit && exit_code != 0) {
            if (opts.is_tty) { (void)fputs("\033[?25h", stdout);
}
            return exit_code;
        }
        if (opts.chgexit && have_prev && output != prev_output) {
            if (opts.is_tty) { (void)fputs("\033[?25h", stdout);
}
            return 0;
        }
        prev_output = output;
        have_prev = true;

        struct timespec ts;
        ts.tv_sec = static_cast<time_t>(opts.interval);
        ts.tv_nsec = static_cast<long>((opts.interval -
                                        static_cast<double>(ts.tv_sec)) *
                                       1e9);
        while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
        }
    }
}

}  // namespace

REGISTER_COMMAND("watch", watch_command,
                 "Execute a program periodically, showing output fullscreen");
