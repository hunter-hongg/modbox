// modbox flock — manage file locks from shell scripts
//
// Behavior modeled on util-linux sys-utils/flock.c (v2.42.3). The core is a
// flock(2) wrapper around a command:
//
//   flock [options] <file|dir> <command> [args...]
//   flock [options] <file|dir> -c <command>
//   flock [options] <file descriptor number>
//
// Options stop at the first non-option argument, mirroring upstream's "+"
// getopt prefix; `--` ends option parsing. The exit code is the command's
// status; a lock conflict or timeout exits with --conflict-exit-code
// (default 1).
//
// Two documented deviations from upstream:
//   - usage errors exit 2 (repo convention) instead of EX_USAGE 64
//   - a failed exec exits 127 (repo launcher convention) instead of 69

#include <cerrno>
#include <climits>
#include <cmath>
#include <limits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/command_macros.hpp"
#include "commands/flock.hpp"
#include "commands/version_util.hpp"

namespace {

constexpr int kExitUsage = 2;  // repo usage-error convention (upstream EX_USAGE 64)
constexpr int kExitExec = 127;  // repo launcher convention (upstream EX_UNAVAILABLE 69)

volatile sig_atomic_t g_timeout_expired = 0;

void timeout_handler(int) { g_timeout_expired = 1; }

void usage(const char* prog) {
    std::printf("Usage: %s [options] <file>|<directory> <command> [<argument>...]\n", prog);
    std::printf("       %s [options] <file>|<directory> -c <command>\n", prog);
    std::printf("       %s [options] <file descriptor number>\n", prog);
    std::printf("\n");
    std::printf("Manage file locks from shell scripts.\n");
    std::printf("\n");
    std::printf(" -s, --shared             get a shared lock\n");
    std::printf(" -x, --exclusive          get an exclusive lock (default)\n");
    std::printf(" -u, --unlock             remove a lock\n");
    std::printf(" -n, --nb, --nonblocking  fail rather than wait\n");
    std::printf(" -w, --timeout <secs>     wait for a limited amount of time\n");
    std::printf(" -E, --conflict-exit-code <number>  exit code after conflict or timeout\n");
    std::printf(" -o, --close              close file descriptor before running command\n");
    std::printf(" -c, --command <command>  run a single command string through the shell\n");
    std::printf(" -F, --no-fork            execute command without forking\n");
    std::printf("     --wait               same as --timeout\n");
    std::printf("     --verbose            increase verbosity\n");
    std::printf("\n");
    std::printf(" -h, --help               display this help\n");
    std::printf(" -V, --version            display version\n");
}

// Usage error with a pointer at the help text.
int usage_error(const char* prog, const std::string& msg) {
    std::fprintf(stderr, "%s: %s\n", prog, msg.c_str());
    std::fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    return kExitUsage;
}

// Usage error without the help pointer, matching upstream errx() wording.
// Takes a std::string so callers can build interpolated messages.
int usage_error_bare(const char* prog, const std::string& msg) {
    std::fprintf(stderr, "%s: %s\n", prog, msg.c_str());
    return kExitUsage;
}

// Parse a fractional-second timeout into timeval. Upstream accepts negative
// values at parse time; the failure occurs at setitimer(2) with EINVAL.
bool parse_timeval(const char* s, timeval& tv) {
    char* end = nullptr;
    errno = 0;
    double const v = std::strtod(s, &end);
    // Reject non-finite and out-of-range values before converting to integer.
    // The upper bound is exclusive: converting time_t::max() to double may
    // round upward to the first unrepresentable integer.
    if (end == s || *end != '\0' || errno == ERANGE || !std::isfinite(v) ||
        v >= static_cast<double>(std::numeric_limits<time_t>::max()) ||
        v < static_cast<double>(std::numeric_limits<time_t>::min())) return false;
    tv.tv_sec = static_cast<time_t>(v);
    tv.tv_usec = static_cast<suseconds_t>((v - static_cast<double>(tv.tv_sec)) * 1000000.0);
    return true;
}

// Open the lock target: a regular file (created if missing) or a directory.
int open_file(const std::string& filename, int& flags, const char* prog) {
    int fl = flags == 0 ? O_RDONLY : flags;
    errno = 0;
    fl |= O_NOCTTY | O_CREAT;
    int fd = open(filename.c_str(), fl, 0666);
    if (fd < 0 && errno == EISDIR) {
        fl = O_RDONLY | O_NOCTTY;
        fd = open(filename.c_str(), fl);
    }
    if (fd < 0) {
        std::fprintf(stderr, "%s: cannot open lock file %s: %s\n", prog, filename.c_str(),
                     std::strerror(errno));
    }
    flags = fl;
    return fd;
}

}  // namespace

int flock_command(int argc, char** argv) {
    const char* prog = "flock";
    int type = LOCK_EX;
    int block = 0;
    int open_flags = 0;
    int fd = -1;
    bool do_close = false;
    bool no_fork = false;
    bool verbose = false;
    bool have_timeout = false;
    timeval timeout {};
    int conflict_exit_code = 1;

    int i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if (std::strcmp(a, "--") == 0) {
            i++;
            break;
        }
        if (a[0] != '-' || a[1] == '\0') break;  // first non-option ends parsing

        // Long options. Upstream registers --command as a SHORT option only,
        // so an unmatched long token is an error, not a positional. It also
        // leaves getopt_long_options unset, which makes unrecognized options
        // render as a bare "--" rather than their full spelling.
        if (a[1] == '-') {
            if (std::strcmp(a, "--help") == 0) {
                usage(prog);
                return 0;
            }
            if (std::strcmp(a, "--version") == 0) {
                print_version("flock");
                return 0;
            }
            bool consumed = false;
            if (std::strcmp(a, "--shared") == 0) {
                type = LOCK_SH;
                consumed = true;
            } else if (std::strcmp(a, "--exclusive") == 0) {
                type = LOCK_EX;
                consumed = true;
            } else if (std::strcmp(a, "--unlock") == 0) {
                type = LOCK_UN;
                consumed = true;
            } else if (std::strcmp(a, "--nonblocking") == 0 || std::strcmp(a, "--nb") == 0) {
                block = LOCK_NB;
                consumed = true;
            } else if (std::strcmp(a, "--close") == 0) {
                do_close = true;
                consumed = true;
            } else if (std::strcmp(a, "--no-fork") == 0) {
                no_fork = true;
                consumed = true;
            } else if (std::strcmp(a, "--verbose") == 0) {
                verbose = true;
                consumed = true;
            } else if (std::strcmp(a, "--wait") == 0 || std::strcmp(a, "--timeout") == 0) {
                if (i + 1 >= argc) return usage_error_bare(prog, "option requires an argument -- 'w'");
                if (!parse_timeval(argv[++i], timeout)) return usage_error(prog, "invalid timeout");
                have_timeout = true;
                consumed = true;
            } else if (std::strcmp(a, "--conflict-exit-code") == 0) {
                if (i + 1 >= argc) return usage_error_bare(prog, "option requires an argument -- 'E'");
                char* end = nullptr;
                long const v = std::strtol(argv[++i], &end, 10);
                if (end == argv[i] || *end != '\0') return usage_error(prog, "invalid exit code");
                if (v < 0 || v > 255) return usage_error_bare(prog, "exit code out of range (expected 0 to 255)");
                conflict_exit_code = static_cast<int>(v);
                consumed = true;
            }
            if (consumed) continue;
            return usage_error(prog, "unrecognized option '--'");
        }

        // Short options, possibly clustered (-sn) or value-attached (-w1.5).
        int matched = 0;
        for (int j = 1; a[j] != '\0'; j++) {
            if (a[j] == 's') {
                type = LOCK_SH;
                matched = 1;
            } else if (a[j] == 'x' || a[j] == 'e') {
                type = LOCK_EX;
                matched = 1;
            } else if (a[j] == 'u') {
                type = LOCK_UN;
                matched = 1;
            } else if (a[j] == 'n') {
                block = LOCK_NB;
                matched = 1;
            } else if (a[j] == 'o') {
                do_close = true;
                matched = 1;
            } else if (a[j] == 'F') {
                no_fork = true;
                matched = 1;
            } else if (a[j] == 'w' || a[j] == 'E') {
                const char* val = a[j + 1] != '\0' ? &a[j + 1] : (i + 1 < argc ? argv[i + 1] : nullptr);
                if (val == nullptr)
                    return usage_error_bare(prog,
                                            "option requires an argument -- '" + std::string(1, a[j]) + "'");
                if (a[j] == 'w') {
                    if (!parse_timeval(val, timeout)) return usage_error(prog, "invalid timeout");
                    have_timeout = true;
                } else {
                    char* end = nullptr;
                    long const v = std::strtol(val, &end, 10);
                    if (end == val || *end != '\0')
                        return usage_error(prog, "invalid exit code: '" + std::string(val) + "'");
                    if (v < 0 || v > 255)
                        return usage_error_bare(prog, "exit code out of range (expected 0 to 255)");
                    conflict_exit_code = static_cast<int>(v);
                }
                if (a[j + 1] == '\0') i++;  // consumed the next argv token
                matched = 1;
                break;
            } else if (a[j] == 'h') {
                usage(prog);
                return 0;
            } else if (a[j] == 'V') {
                print_version("flock");
                return 0;
            } else {
                char msg[80];
                std::snprintf(msg, sizeof(msg), "%s: unrecognized option '-%c'", prog, a[j]);
                return usage_error_bare(prog, msg);
            }
        }
        if (matched == 0) break;
    }

    if (no_fork && do_close)
        return usage_error_bare(prog, "the --no-fork and --close options are incompatible");

    // Dispatch (mirrors upstream exactly):
    //   more than one positional  -> file form, argv[i] is the path, the rest is the command
    //   exactly one positional    -> fd form, argv[i] must be a file descriptor number
    //   no positional             -> usage error
    // Consequence worth knowing: with two or more positionals a non-numeric
    // first argument is treated as a FILE NAME, not as a bad fd.
    std::string target;
    char** cmd_argv = nullptr;
    std::string shell_cmd;
    bool shell_form = false;
    bool have_cmd = false;
    if (i + 1 < argc) {
        target = argv[i];
        const char* cmd0 = argv[i + 1];
        if (std::strcmp(cmd0, "-c") == 0 || std::strcmp(cmd0, "--command") == 0) {
            if (i + 3 != argc)
                return usage_error_bare(prog, "-c requires exactly one command argument");
            shell_cmd = argv[i + 2];
            shell_form = true;
            have_cmd = true;
        } else {
            cmd_argv = &argv[i + 1];
            have_cmd = true;
        }
        fd = open_file(target, open_flags, prog);
        if (fd < 0) return 1;
    } else if (i < argc) {
        // Single argument: a numeric file descriptor. No command to run.
        target = argv[i];
        char* end = nullptr;
        errno = 0;
        long const fdnum = std::strtol(target.c_str(), &end, 10);
        if (end == target.c_str() || *end != '\0' || fdnum < 0 || fdnum > INT_MAX)
            return usage_error_bare(prog, "bad file descriptor: '" + target + "'");
        fd = static_cast<int>(fdnum);
    } else {
        return usage_error(prog, "not enough arguments");
    }

    if (have_timeout) {
        if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
            have_timeout = false;
            block = LOCK_NB;  // -w 0 is equivalent to -n; a zero itimer means disabled
        } else {
            struct sigaction sa {};
            sa.sa_handler = timeout_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGALRM, &sa, nullptr);
            itimerval it {};
            it.it_value = timeout;
            if (setitimer(ITIMER_REAL, &it, nullptr) != 0) {
                std::fprintf(stderr, "%s: cannot set up timer: %s\n", prog,
                             std::strerror(errno));
                return 71;
            }
        }
    }

    struct timespec time_start {};
    struct timespec time_done {};
    if (verbose) clock_gettime(CLOCK_MONOTONIC, &time_start);

    while (flock(fd, type | block) != 0) {
        switch (errno) {
            case EWOULDBLOCK:
            case EACCES:
                if (verbose) std::fprintf(stderr, "%s: failed to get lock\n", prog);
                if (have_timeout) {
                    itimerval off {};
                    setitimer(ITIMER_REAL, &off, nullptr);
                }
                return conflict_exit_code;
            case EINTR:
                if (g_timeout_expired) {
                    if (verbose)
                        std::fprintf(stderr, "%s: timeout while waiting to get lock\n", prog);
                    return conflict_exit_code;
                }
                continue;
            default:
                std::fprintf(stderr, "%s: %d: %s\n", prog, fd, std::strerror(errno));
                if (have_timeout) {
                    itimerval off {};
                    setitimer(ITIMER_REAL, &off, nullptr);
                }
                return 1;
        }
    }

    if (have_timeout) {
        itimerval off {};
        setitimer(ITIMER_REAL, &off, nullptr);
    }

    if (verbose) {
        clock_gettime(CLOCK_MONOTONIC, &time_done);
        long sec = time_done.tv_sec - time_start.tv_sec;
        long nsec = time_done.tv_nsec - time_start.tv_nsec;
        if (nsec < 0) {
            nsec += 1000000000L;
            sec -= 1;
        }
        std::fprintf(stdout, "%s: getting lock took %ld.%06ld seconds\n", prog, sec,
                     nsec / 1000);
    }

    if (!have_cmd) {
        // fd form, no command: nothing to run.
        return 0;
    }

    const char* shell = std::getenv("SHELL");
    if (shell == nullptr || shell[0] == '\0') shell = "/bin/sh";

    if (verbose) {
        std::fprintf(stdout, "%s: executing %s\n", prog, shell_form ? shell : cmd_argv[0]);
        // Preserve diagnostic ordering with redirected stdout and don't lose
        // buffered diagnostics when --no-fork replaces this process.
        std::fflush(stdout);
    }

    if (!no_fork) {
        pid_t const child = fork();
        if (child < 0) {
            std::fprintf(stderr, "%s: fork failed: %s\n", prog, std::strerror(errno));
            return 1;
        }
        if (child == 0) {
            // Normally the command inherits the lock descriptor across exec.
            // With --close, only the waiting parent retains this descriptor
            // and holds the lock until the command exits.
            if (do_close) close(fd);
            if (shell_form) {
                execl(shell, shell, "-c", shell_cmd.c_str(), static_cast<char*>(nullptr));
            } else {
                execvp(cmd_argv[0], cmd_argv);
            }
            std::fprintf(stderr, "%s: failed to execute %s: %s\n", prog,
                         shell_form ? shell : cmd_argv[0], std::strerror(errno));
            _exit(kExitExec);
        }
        int status = 0;
        pid_t w;
        do {
            w = waitpid(child, &status, 0);
        } while (w == -1 && errno == EINTR);
        if (w == -1) {
            std::fprintf(stderr, "%s: waitpid failed: %s\n", prog, std::strerror(errno));
            return 1;
        }
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return 1;
    }

    // no-fork: exec in place.
    if (do_close) close(fd);
    if (shell_form) {
        execl(shell, shell, "-c", shell_cmd.c_str(), static_cast<char*>(nullptr));
    } else {
        execvp(cmd_argv[0], cmd_argv);
    }
    std::fprintf(stderr, "%s: failed to execute %s: %s\n", prog,
                 shell_form ? shell : cmd_argv[0], std::strerror(errno));
    return kExitExec;
}

REGISTER_COMMAND("flock", flock_command, "Manage file locks from shell scripts");
