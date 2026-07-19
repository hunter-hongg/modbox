#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <csignal>
#include <ctime>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/timeout.hpp"

static const int EXIT_TIMEDOUT = 124;
static const int EXIT_CANCELED = 125;
static const int EXIT_CANNOT_INVOKE = 126;
static const int EXIT_ENOENT = 127;

static pid_t child_pid = 0;

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION] DURATION COMMAND [ARG]...\n", prog);
    printf("  or:  %s [OPTION]\n", prog);
    printf("Start COMMAND, and kill it if still running after DURATION.\n");
    printf("\n");
    printf("      --preserve-status\n");
    printf("                 exit with the same status as COMMAND, even when the\n");
    printf("                 command times out\n");
    printf("      --foreground\n");
    printf("                 when not running timeout directly from a shell prompt,\n");
    printf("                 allow COMMAND to read from the TTY and get TTY signals;\n");
    printf("                 in this mode, children of COMMAND will not be timed out\n");
    printf("      --kill-after=DURATION\n");
    printf("                 also send a KILL signal if COMMAND is still running\n");
    printf("                 this long after the initial signal was sent\n");
    printf("      --signal=SIGNAL\n");
    printf("                 specify the signal to be sent on timeout;\n");
    printf("                 SIGNAL may be a name like 'HUP' or a number;\n");
    printf("                 see 'kill -l' for a list of signals\n");
    printf("  -v, --verbose  diagnose to stderr any signal sent upon timeout\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
    printf("\n");
    printf("DURATION is a floating point number with an optional suffix:\n");
    printf("'s' for seconds (the default), 'm' for minutes, 'h' for hours or 'd' for days.\n");
    printf("\n");
    printf("If the command times out, and --preserve-status is not set, then exit with\n");
    printf("status 124.  Otherwise, the exit status of COMMAND is used.\n");
}

static void usage_error(const char* prog) {
    fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    exit(EXIT_CANCELED);
}

static bool parse_duration(const char* s, double* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }

    char* endp = nullptr;
    errno = 0;
    double val = strtod(s, &endp);

    if (endp == s) {
        return false;
    }

    double multiplier = 1.0;
    if (*endp != '\0') {
        const char suffix = *endp;
        if (suffix == 's') {
            multiplier = 1.0;
        } else if (suffix == 'm') {
            multiplier = 60.0;
        } else if (suffix == 'h') {
            multiplier = 3600.0;
        } else if (suffix == 'd') {
            multiplier = 86400.0;
        } else {
            return false;
        }
        endp++;
        if (*endp != '\0') {
            return false;
        }
    }

    if (val < 0) {
        return false;
    }

    *out = val * multiplier;
    return true;
}

static int parse_signal(const char* name) {
    if (name == nullptr || *name == '\0') {
        return -1;
    }

    if (*name >= '0' && *name <= '9') {
        errno = 0;
        char* endp = nullptr;
        long val = strtol(name, &endp, 10);
        if (endp == name || *endp != '\0' || val < 0 || val > 255) {
            return -1;
        }
        return (int)val;
    }

    std::string s(name);
    if (s.size() > 3 && s.compare(0, 3, "SIG", 3) == 0) {
        s = s.substr(3);
    }

    if (s == "HUP") return SIGHUP;
    if (s == "INT") return SIGINT;
    if (s == "QUIT") return SIGQUIT;
    if (s == "ILL") return SIGILL;
    if (s == "TRAP") return SIGTRAP;
    if (s == "ABRT") return SIGABRT;
    if (s == "BUS") return SIGBUS;
    if (s == "FPE") return SIGFPE;
    if (s == "KILL") return SIGKILL;
    if (s == "USR1") return SIGUSR1;
    if (s == "SEGV") return SIGSEGV;
    if (s == "USR2") return SIGUSR2;
    if (s == "PIPE") return SIGPIPE;
    if (s == "ALRM") return SIGALRM;
    if (s == "TERM") return SIGTERM;
    if (s == "CHLD") return SIGCHLD;
    if (s == "CONT") return SIGCONT;
    if (s == "STOP") return SIGSTOP;
    if (s == "TSTP") return SIGTSTP;
    if (s == "TTIN") return SIGTTIN;
    if (s == "TTOU") return SIGTTOU;
    if (s == "URG") return SIGURG;
    if (s == "XCPU") return SIGXCPU;
    if (s == "XFSZ") return SIGXFSZ;
    if (s == "VTALRM") return SIGVTALRM;
    if (s == "PROF") return SIGPROF;
    if (s == "WINCH") return SIGWINCH;
    if (s == "IO" || s == "POLL") return SIGIO;
    if (s == "SYS") return SIGSYS;

    return -1;
}

static void send_signal_verbose(const char* prog, int sig) {
    fprintf(stderr, "%s: sending signal %d to command '%s'\n", prog, sig, "");
}

void timeout_command(int argc, char** argv) {
    const char* prog = argv[0];

    bool preserve_status = false;
    bool foreground = false;
    bool verbose = false;
    double kill_after = 0.0;
    bool have_kill_after = false;
    int timeout_signal = SIGTERM;

    int i = 1;

    for (; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--") == 0) {
            i++;
            break;
        }
        if (strcmp(a, "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("timeout (modbox) 1.0\n");
            return;
        }
        if (strcmp(a, "--preserve-status") == 0) {
            preserve_status = true;
            continue;
        }
        if (strcmp(a, "--foreground") == 0) {
            foreground = true;
            continue;
        }
        if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) {
            verbose = true;
            continue;
        }
        if (strncmp(a, "--kill-after=", 13) == 0) {
            if (!parse_duration(a + 13, &kill_after) || kill_after < 0) {
                fprintf(stderr, "%s: invalid time interval '%s'\n", prog, a + 13);
                usage_error(prog);
            }
            have_kill_after = true;
            continue;
        }
        if (strncmp(a, "--signal=", 9) == 0) {
            timeout_signal = parse_signal(a + 9);
            if (timeout_signal < 0) {
                fprintf(stderr, "%s: invalid signal '%s'\n", prog, a + 9);
                usage_error(prog);
            }
            continue;
        }
        if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "%s: unrecognized option '%s'\n", prog, a);
            usage_error(prog);
        }
        break;
    }

    if (i >= argc) {
        fprintf(stderr, "%s: missing operand\n", prog);
        usage_error(prog);
    }

    double duration = 0.0;
    if (!parse_duration(argv[i], &duration) || duration < 0) {
        fprintf(stderr, "%s: invalid time interval '%s'\n", prog, argv[i]);
        usage_error(prog);
    }
    i++;

    if (i >= argc) {
        fprintf(stderr, "%s: missing command\n", prog);
        usage_error(prog);
    }

    int timeout_sec = (int)duration;
    long timeout_nsec = (long)((duration - (double)timeout_sec) * 1000000000.0);

    child_pid = fork();
    if (child_pid < 0) {
        fprintf(stderr, "%s: fork failed: %s\n", prog, strerror(errno));
        exit(EXIT_CANCELED);
    }

    if (child_pid == 0) {
        execvp(argv[i], &argv[i]);
        int code = (errno == ENOENT) ? EXIT_ENOENT : EXIT_CANNOT_INVOKE;
        fprintf(stderr, "%s: failed to run command '%s': %s\n",
                prog, argv[i], strerror(errno));
        _exit(code);
    }

    struct timespec ts;
    ts.tv_sec = timeout_sec;
    ts.tv_nsec = timeout_nsec;

    bool timed_out = false;

    if (timeout_sec == 0 && timeout_nsec == 0) {
        timed_out = true;
    } else {
        struct timespec remaining = ts;
        while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR) {
            int status;
            pid_t r = waitpid(child_pid, &status, WNOHANG);
            if (r == child_pid) {
                if (WIFEXITED(status)) {
                    exit(preserve_status ? WEXITSTATUS(status) : WEXITSTATUS(status));
                }
                if (WIFSIGNALED(status)) {
                    exit(preserve_status ? 128 + WTERMSIG(status) : WEXITSTATUS(status));
                }
            }
        }

        int status;
        pid_t r = waitpid(child_pid, &status, WNOHANG);
        if (r == child_pid) {
            if (WIFEXITED(status)) {
                exit(preserve_status ? WEXITSTATUS(status) : WEXITSTATUS(status));
            }
            if (WIFSIGNALED(status)) {
                exit(preserve_status ? 128 + WTERMSIG(status) : WEXITSTATUS(status));
            }
        }
        timed_out = true;
    }

    if (timed_out) {
        if (verbose) {
            send_signal_verbose(prog, timeout_signal);
        }
        kill(child_pid, timeout_signal);

        if (have_kill_after && kill_after > 0) {
            int kill_sec = (int)kill_after;
            long kill_nsec = (long)((kill_after - (double)kill_sec) * 1000000000.0);
            struct timespec kts;
            kts.tv_sec = kill_sec;
            kts.tv_nsec = kill_nsec;

            struct timespec kremaining = kts;
            while (nanosleep(&kremaining, &kremaining) == -1 && errno == EINTR) {
                continue;
            }

            int status;
            pid_t r = waitpid(child_pid, &status, WNOHANG);
            if (r != child_pid) {
                if (verbose) {
                    send_signal_verbose(prog, SIGKILL);
                }
                kill(child_pid, SIGKILL);
            }
        }

        int status;
        while (waitpid(child_pid, &status, 0) == -1 && errno == EINTR) {
            continue;
        }

        if (preserve_status) {
            if (WIFEXITED(status)) {
                exit(WEXITSTATUS(status));
            }
            if (WIFSIGNALED(status)) {
                if (WTERMSIG(status) == timeout_signal) {
                    exit(EXIT_TIMEDOUT);
                }
                exit(128 + WTERMSIG(status));
            }
        }
        exit(EXIT_TIMEDOUT);
    }
}
