#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <signal.h>
#include <string>
#include <vector>
#include <sys/types.h>

#include "commands/kill.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [-s SIGNAL|SIGNAL] PID...\n", prog);
    printf("Send signal to process(es).\n");
    printf("\n");
    printf("  -s, --signal=SIGNAL   signal to send (NAME or NUMBER)\n");
    printf("  -l, --list            list signal names\n");
    printf("  -L, --list            list signal names\n");
    printf("  -p, --pid             only print PID, do not send signal\n");
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n");
}

static void print_signals() {
    printf(" 1) SIGHUP     2) SIGINT     3) SIGQUIT    4) SIGILL     5) SIGTRAP\n");
    printf(" 6) SIGABRT    7) SIGBUS     8) SIGFPE     9) SIGKILL   10) SIGUSR1\n");
    printf("11) SIGSEGV   12) SIGUSR2   13) SIGPIPE   14) SIGALRM   15) SIGTERM\n");
    printf("16) SIGSTKFLT 17) SIGCHLD   18) SIGCONT   19) SIGSTOP   20) SIGTSTP\n");
    printf("21) SIGTTIN   22) SIGTTOU   23) SIGURG    24) SIGXCPU   25) SIGXFSZ\n");
    printf("26) SIGVTALRM 27) SIGPROF   28) SIGWINCH  29) SIGIO     30) SIGPWR\n");
    printf("31) SIGSYS\n");
}

// Convert a signal number to its short name (e.g. 15 -> "TERM"), or nullptr.
static const char* signal_short_name(int sig) {
    switch (sig) {
        case SIGHUP:    return "HUP";
        case SIGINT:    return "INT";
        case SIGQUIT:   return "QUIT";
        case SIGILL:    return "ILL";
        case SIGTRAP:   return "TRAP";
        case SIGABRT:   return "ABRT";
        case SIGBUS:    return "BUS";
        case SIGFPE:    return "FPE";
        case SIGKILL:   return "KILL";
        case SIGUSR1:   return "USR1";
        case SIGSEGV:   return "SEGV";
        case SIGUSR2:   return "USR2";
        case SIGPIPE:   return "PIPE";
        case SIGALRM:   return "ALRM";
        case SIGTERM:   return "TERM";
        case SIGCHLD:   return "CHLD";
        case SIGCONT:   return "CONT";
        case SIGSTOP:   return "STOP";
        case SIGTSTP:   return "TSTP";
        case SIGTTIN:   return "TTIN";
        case SIGTTOU:   return "TTOU";
        case SIGURG:    return "URG";
        case SIGXCPU:   return "XCPU";
        case SIGXFSZ:   return "XFSZ";
        case SIGVTALRM: return "VTALRM";
        case SIGPROF:   return "PROF";
        case SIGWINCH:  return "WINCH";
        case SIGIO:     return "IO";
        case SIGPWR:    return "PWR";
        case SIGSYS:    return "SYS";
        default:        return nullptr;
    }
}

static int resolve_signal(const char* name) {
    if (name[0] >= '0' && name[0] <= '9') {
        int const sig = std::atoi(name);
        if (sig >= 0 && sig < 64) { return sig;
}
        return -1;
    }
    size_t const len = std::strlen(name);
    if (len > 3 && std::strncmp(name, "SIG", 3) == 0) { name += 3;
}
    if (std::strcmp(name, "HUP") == 0) { return SIGHUP;
}
    if (std::strcmp(name, "INT") == 0) { return SIGINT;
}
    if (std::strcmp(name, "QUIT") == 0) { return SIGQUIT;
}
    if (std::strcmp(name, "ILL") == 0) { return SIGILL;
}
    if (std::strcmp(name, "TRAP") == 0) { return SIGTRAP;
}
    if (std::strcmp(name, "ABRT") == 0) { return SIGABRT;
}
    if (std::strcmp(name, "BUS") == 0) { return SIGBUS;
}
    if (std::strcmp(name, "FPE") == 0) { return SIGFPE;
}
    if (std::strcmp(name, "KILL") == 0) { return SIGKILL;
}
    if (std::strcmp(name, "USR1") == 0) { return SIGUSR1;
}
    if (std::strcmp(name, "SEGV") == 0) { return SIGSEGV;
}
    if (std::strcmp(name, "USR2") == 0) { return SIGUSR2;
}
    if (std::strcmp(name, "PIPE") == 0) { return SIGPIPE;
}
    if (std::strcmp(name, "ALRM") == 0) { return SIGALRM;
}
    if (std::strcmp(name, "TERM") == 0) { return SIGTERM;
}
    if (std::strcmp(name, "CHLD") == 0) { return SIGCHLD;
}
    if (std::strcmp(name, "CONT") == 0) { return SIGCONT;
}
    if (std::strcmp(name, "STOP") == 0) { return SIGSTOP;
}
    if (std::strcmp(name, "TSTP") == 0) { return SIGTSTP;
}
    if (std::strcmp(name, "TTIN") == 0) { return SIGTTIN;
}
    if (std::strcmp(name, "TTOU") == 0) { return SIGTTOU;
}
    if (std::strcmp(name, "URG") == 0) { return SIGURG;
}
    if (std::strcmp(name, "XCPU") == 0) { return SIGXCPU;
}
    if (std::strcmp(name, "XFSZ") == 0) { return SIGXFSZ;
}
    if (std::strcmp(name, "VTALRM") == 0) { return SIGVTALRM;
}
    if (std::strcmp(name, "PROF") == 0) { return SIGPROF;
}
    if (std::strcmp(name, "WINCH") == 0) { return SIGWINCH;
}
    if (std::strcmp(name, "IO") == 0 || std::strcmp(name, "POLL") == 0) { return SIGIO;
}
    if (std::strcmp(name, "PWR") == 0) { return SIGPWR;
}
    if (std::strcmp(name, "SYS") == 0) { return SIGSYS;
}
    return -1;
}

int kill_command(int argc, char** argv) {
    int sig = SIGTERM;
    bool list_signals = false;
    bool print_only = false;
    bool have_signal = false;  // signal explicitly selected via -s / -SIG
    std::vector<pid_t> pids;
    std::vector<std::string> list_args;  // operands that follow -l

    int i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if (std::strcmp(a, "--help") == 0) { print_help(argv[0]); return 0; }
        if (std::strcmp(a, "--version") == 0) { print_version("kill"); return 0; }
        if (std::strcmp(a, "-l") == 0 || std::strcmp(a, "-L") == 0) {
            list_signals = true;
            continue;
        }
        if (std::strcmp(a, "-p") == 0) { print_only = true; continue; }
        if ((std::strcmp(a, "-s") == 0 || std::strcmp(a, "--signal") == 0) &&
            i + 1 < argc) {
            int const s = resolve_signal(argv[++i]);
            if (s < 0) {
                (void)fprintf(stderr, "kill: invalid signal: %s\n", argv[i]);
                return 1;
            }
            sig = s;
            have_signal = true;
            continue;
        }
        if (a[0] == '-' && a[1] >= '0' && a[1] <= '9') {
            int const s = std::atoi(a + 1);
            if (s >= 0 && s < 64) { sig = s; have_signal = true; continue; }
        }
        // In list mode, operands are signals to translate, not PIDs.
        if (list_signals) {
            list_args.emplace_back(a);
            continue;
        }
        char* end = nullptr;
        long pid_val = std::strtol(a, &end, 10);
        if ((end != nullptr) && *end == '\0' && pid_val > 0) {
            pids.push_back(static_cast<pid_t>(pid_val));
        } else {
            (void)fprintf(stderr, "kill: invalid pid: %s\n", a);
            return 1;
        }
    }

    if (list_signals) {
        if (list_args.empty()) {
            print_signals();
            return 0;
        }
        int rc = 0;
        for (const std::string& arg : list_args) {
            int const s = resolve_signal(arg.c_str());
            if (s > 0) {
                const char* name = signal_short_name(s);
                if (name != nullptr) {
                    printf("%s\n", name);
                } else {
                    (void)fprintf(stderr, "kill: unknown signal: %s\n", arg.c_str());
                    rc = 1;
                }
            } else {
                (void)fprintf(stderr, "kill: unknown signal: %s\n", arg.c_str());
                rc = 1;
            }
        }
        return rc;
    }

    if (pids.empty()) {
        (void)fprintf(stderr, "kill: missing operand\n");
        return 1;
    }

    (void)have_signal;  // default SIGTERM when not specified

    int rc = 0;
    for (pid_t pid : pids) {
        if (print_only) {
            printf("%d\n", pid);
            continue;
        }
        if (kill(pid, sig) < 0) {
            (void)fprintf(stderr, "kill: (%d) - %s\n", pid, strerror(errno));
            rc = 1;
        }
    }
    return rc;
}

REGISTER_COMMAND("kill", kill_command, "Terminate process");
