#include <argtable3.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "commands/pidof.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

struct ProcEntry {
    pid_t pid = 0;
    std::string comm;        // /proc/[pid]/comm, newline stripped
    std::string exe;         // /proc/[pid]/exe target; empty when unreadable
    std::vector<std::string> argv;  // /proc/[pid]/cmdline, NUL-split
};

std::string read_file_first_line(const char* path) {
    std::string out;
    FILE* fp = fopen(path, "r");
    if (fp == nullptr) {
        return out;
    }
    char buf[512];
    if (fgets(buf, sizeof(buf), fp) != nullptr) {
        out = buf;
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
            out.pop_back();
        }
    }
    (void)fclose(fp);
    return out;
}

std::string readlink_or_empty(const char* path) {
    char buf[4096];
    const ssize_t len = readlink(path, buf, sizeof(buf) - 1);
    if (len < 0) {
        return {};
    }
    buf[len] = '\0';
    return std::string(buf);
}

// Read /proc/[pid]/cmdline, splitting on the NUL separators.
std::vector<std::string> read_cmdline(pid_t pid) {
    std::vector<std::string> argv;
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE* fp = fopen(path, "r");
    if (fp == nullptr) {
        return argv;
    }
    std::string cur;
    int c = 0;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\0') {
            if (!cur.empty()) {
                argv.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(static_cast<char>(c));
        }
    }
    if (!cur.empty()) {
        argv.push_back(cur);
    }
    (void)fclose(fp);
    return argv;
}

bool is_numeric(const char* s) {
    if (*s == '\0') {
        return false;
    }
    for (const char* p = s; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    return true;
}

// Final path component of a (possibly slashed) path.
std::string base_name(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

// procps recognises a fixed set of shells for -x script matching.
bool is_shell_name(const std::string& name) {
    static const char* const shells[] = {"sh", "bash", "dash", "ash", "ksh",
                                         "zsh", "csh", "tcsh", "fish", nullptr};
    for (int i = 0; shells[i] != nullptr; i++) {
        if (name == shells[i]) {
            return true;
        }
    }
    return false;
}

// Does a "-x" shell invocation run the named program as its script?
bool script_matches(const ProcEntry& p, const std::string& program) {
    if (p.argv.empty()) {
        return false;
    }
    if (!is_shell_name(base_name(p.argv[0]))) {
        return false;
    }
    for (size_t i = 1; i < p.argv.size(); i++) {
        // Skip option arguments like "-c" / "-lc".
        if (!p.argv[i].empty() && p.argv[i][0] == '-' && i + 1 < p.argv.size()) {
            i++;
            continue;
        }
        if (base_name(p.argv[i]) == program) {
            return true;
        }
        break;  // only the first operand names the script
    }
    return false;
}

bool program_matches(const ProcEntry& p, const std::string& program, bool with_scripts) {
    // A program argument containing a slash is matched against the exact
    // executable path; a bare name is matched against its basename (or,
    // when the executable link is unreadable, against comm).
    if (program.find('/') != std::string::npos) {
        return p.exe == program;
    }
    if (!p.exe.empty()) {
        if (base_name(p.exe) == program) {
            return true;
        }
    } else if (p.comm == program) {
        return true;
    }
    return with_scripts && script_matches(p, program);
}

std::vector<ProcEntry> enumerate_processes() {
    std::vector<ProcEntry> out;
    DIR* d = opendir("/proc");
    if (d == nullptr) {
        return out;
    }
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr) {
        if (!is_numeric(ent->d_name)) {
            continue;
        }
        const pid_t pid = static_cast<pid_t>(atoi(ent->d_name));
        ProcEntry p;
        p.pid = pid;
        char path[64];
        (void)snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        p.comm = read_file_first_line(path);
        (void)snprintf(path, sizeof(path), "/proc/%d/exe", pid);
        p.exe = readlink_or_empty(path);
        p.argv = read_cmdline(pid);
        // The process may have exited between readdir and the reads above.
        if (p.comm.empty() && p.exe.empty() && p.argv.empty()) {
            continue;
        }
        out.push_back(std::move(p));
    }
    closedir(d);
    return out;
}

// Parse a -o argument: one PID per call, a comma-separated list, or %PPID.
//
// Deliberate divergence from procps-ng pidof (verified differentially
// against procps-ng 4.0.7): upstream accepts any strtoul-parsable chunk
// as a legal omit entry — including "0" (a no-op, since no live process
// has PID 0), leading-zero forms, and values above pid_max — and only
// warns ("illegal omit pid value (X)!") on garbage while continuing.
// modbox instead rejects the whole value up front with an error and
// exit 1: values must be positive decimal PIDs at or below the kernel's
// pid_max ceiling, or %PPID. Bare "PPID" (no %) is accepted as a
// convenience superset of upstream's "%PPID"-only form. The rationale:
// a silent no-op or warn-and-continue hides typos, and omitting a
// non-existent PID is almost certainly a caller bug worth failing on.
bool parse_omit(const char* spec, std::unordered_set<pid_t>& omit) {
    // The kernel's hard pid_max ceiling (PID_MAX_LIMIT).
    constexpr long PID_MAX_LIMIT = 4194304;
    if (strcmp(spec, "%PPID") == 0 || strcmp(spec, "PPID") == 0) {
        omit.insert(getppid());
        return true;
    }
    const char* p = spec;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return false;
        }
        char* end = nullptr;
        const long v = strtol(p, &end, 10);
        if (v <= 0 || v > PID_MAX_LIMIT) {
            return false;
        }
        omit.insert(static_cast<pid_t>(v));
        if (*end == '\0') {
            break;
        }
        if (*end != ',') {
            return false;
        }
        p = end + 1;
    }
    return true;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int run_pidof(int argc, char** argv) {
    struct arg_lit* single_opt = arg_lit0("s", "single-shot", "return one PID only");
    struct arg_lit* quiet_opt = arg_lit0("q", nullptr, "quiet mode, only set the exit code");
    struct arg_lit* scripts_opt = arg_lit0("x", nullptr, "also find shells running the named scripts");
    struct arg_str* omit_opt = arg_strn("o", "omit-pid", "PID", 0, 1000,
                                        "omit processes with PID (or %PPID), repeatable or comma list");
    struct arg_str* sep_opt = arg_str0("S", "separator", "SEP", "use SEP as separator put between PIDs");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_str* program_arg = arg_strn(nullptr, nullptr, "PROGRAM", 0, 1000,
                                           "program name(s) to look up");
    struct arg_end* end = arg_end(20);

    ArgTable at({single_opt, quiet_opt, scripts_opt, omit_opt, sep_opt,
                 version_opt, help_opt, program_arg, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTIONS] PROGRAM...\n", argv[0]);
        printf("List the process IDs of running programs, highest PID first.\n");
        printf("\n");
        printf("  -s, --single-shot     return one PID only\n");
        printf("  -q                    quiet mode, only set the exit code\n");
        printf("  -x                    also find shells running the named scripts\n");
        printf("  -o, --omit-pid=PID    omit processes with PID (repeatable, comma\n");
        printf("                        separated, or %%PPID to omit the parent)\n");
        printf("  -S, --separator=SEP   use SEP as separator put between PIDs\n");
        printf("  -h, --help            display this help and exit\n");
        printf("  -V, --version         output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("pidof");
        return 0;
    }

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (program_arg->count == 0) {
        (void)fprintf(stderr, "%s: no program name given\n", argv[0]);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    std::unordered_set<pid_t> omit;
    for (int i = 0; i < omit_opt->count; i++) {
        if (!parse_omit(omit_opt->sval[i], omit)) {
            (void)fprintf(stderr, "%s: invalid omit-pid value '%s'\n", argv[0],
                          omit_opt->sval[i]);
            return 1;
        }
    }

    const bool with_scripts = (scripts_opt->count > 0);
    const std::string sep = (sep_opt->count > 0) ? sep_opt->sval[0] : " ";

    std::vector<pid_t> hits;
    for (const ProcEntry& p : enumerate_processes()) {
        if (p.pid == getpid() || omit.find(p.pid) != omit.end()) {
            continue;
        }
        for (int i = 0; i < program_arg->count; i++) {
            if (program_matches(p, program_arg->sval[i], with_scripts)) {
                hits.push_back(p.pid);
                break;
            }
        }
    }

    // procps pidof lists the most recently started (highest) PID first.
    std::sort(hits.begin(), hits.end(), [](pid_t a, pid_t b) { return a > b; });

    if (quiet_opt->count == 0 && !hits.empty()) {
        const size_t shown = (single_opt->count > 0) ? 1 : hits.size();
        for (size_t i = 0; i < shown; i++) {
            if (i != 0) {
                (void)fputs(sep.c_str(), stdout);
            }
            printf("%d", static_cast<int>(hits[i]));
        }
        putchar('\n');
    }

    return hits.empty() ? 1 : 0;
}

}  // namespace

int pidof_command(int argc, char** argv) { return run_pidof(argc, argv); }

REGISTER_COMMAND("pidof", pidof_command, "List PIDs of running programs");
