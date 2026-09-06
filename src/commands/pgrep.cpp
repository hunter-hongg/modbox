#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <argtable3.h>
#include <dirent.h>
#include <pwd.h>
#include <string>
#include <sys/types.h>
#include <vector>
#include "commands/pgrep.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

struct ProcEntry {
    pid_t pid = 0;
    uid_t uid = 0;
    char comm[256] = {0};
    char cmdline[4096] = {0};
};

static bool is_numeric(const char* s) {
    if (s == nullptr) { return false; }
    const char* p = s;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return false;
        }
        ++p;
    }
    return *s != '\0';
}

// Strip a trailing newline in place, bounded by buf_size.
static void chomp(char* buf, size_t buf_size) {
    size_t i = 0;
    while (i < buf_size && buf[i] != '\0') {
        ++i;
    }
    while (i > 0 && (buf[i - 1] == '\n' || buf[i - 1] == '\r')) {
        buf[--i] = '\0';
    }
}

static void read_proc_pid(pid_t pid, ProcEntry& e) {
    e.pid = pid;
    char path[256];

    (void)snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    {
        FILE* fp = fopen(path, "r");
        if (fp != nullptr) {
            if (fgets(e.comm, sizeof(e.comm), fp) != nullptr) {
                chomp(e.comm, sizeof(e.comm));
            }
            fclose(fp);
        }
    }

    (void)snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    {
        FILE* fp = fopen(path, "r");
        if (fp != nullptr) {
            size_t idx = 0;
            int c = 0;
            while ((c = fgetc(fp)) != EOF && idx < sizeof(e.cmdline) - 2) {
                if (c == '\0') {
                    if (idx > 0 && e.cmdline[idx - 1] != ' ') {
                        e.cmdline[idx++] = ' ';
                    }
                } else {
                    e.cmdline[idx++] = static_cast<char>(c);
                }
            }
            e.cmdline[idx] = '\0';
            fclose(fp);
        }
    }

    (void)snprintf(path, sizeof(path), "/proc/%d/status", pid);
    {
        FILE* fp = fopen(path, "r");
        if (fp != nullptr) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != nullptr) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    unsigned int val = 0;
                    if (sscanf(line, "Uid:\t%u", &val) == 1) {
                        e.uid = static_cast<uid_t>(val);
                    }
                    break;
                }
            }
            fclose(fp);
        }
    }
}

static std::vector<ProcEntry> enumerate_processes() {
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
        ProcEntry e;
        read_proc_pid(static_cast<pid_t>(atoi(ent->d_name)), e);
        if (e.pid <= 0) {
            continue;
        }
        out.push_back(e);
    }
    closedir(d);
    return out;
}

static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// The string the pattern is matched against. --full uses the command line,
// falling back to comm for kernel threads, which have an empty cmdline.
static std::string match_target(const ProcEntry& p, bool full_cmd) {
    if (full_cmd && p.cmdline[0] != '\0') {
        return std::string(p.cmdline);
    }
    return std::string(p.comm);
}

int pgrep_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* list_name_opt = arg_lit0("l", "list-name", "print PID and process name");
    struct arg_lit* list_full_opt = arg_lit0("a", "list-full", "print PID and full command line");
    struct arg_lit* full_opt = arg_lit0("f", "full", "match against full command line");
    struct arg_lit* ignore_case_opt = arg_lit0("i", "ignore-case", "case-insensitive matching");
    struct arg_lit* invert_opt = arg_lit0("v", "invert-match", "select non-matching processes");
    struct arg_lit* exact_opt = arg_lit0("x", "exact", "match whole command name");
    struct arg_lit* count_opt = arg_lit0("c", "count", "print only the count of matches");
    struct arg_str* user_opt = arg_str0("u", "user", "USER", "match by user name or ID");
    struct arg_str* pattern_arg =
        arg_strn(NULL, NULL, "PATTERN", 0, 2, "pattern to match against");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, list_name_opt, list_full_opt, full_opt,
                 ignore_case_opt, invert_opt, exact_opt, count_opt, user_opt,
                 pattern_arg, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTIONS]... PATTERN\n", argv[0]);
        printf("List process IDs of processes matching PATTERN.\n");
        printf("\n");
        printf("  -l, --list-name       print PID and process name\n");
        printf("  -a, --list-full       print PID and full command line\n");
        printf("  -f, --full            match against the full command line\n");
        printf("  -i, --ignore-case     case-insensitive matching\n");
        printf("  -v, --invert-match    select non-matching processes\n");
        printf("  -x, --exact           match whole command name\n");
        printf("  -u, --user USER       match by user name or ID\n");
        printf("  -c, --count           print only the count of matches\n");
        printf("\n");
        printf("  -h, --help            display this help and exit\n");
        printf("      --version         output version information and exit\n");
        printf("\n");
        printf("Exits 0 if a process matched, 1 if none did.\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("pgrep");
        return 0;
    }

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }
    if (pattern_arg->count > 1) {
        (void)fprintf(stderr, "pgrep: only one pattern can be specified\n");
        return 2;
    }
    if (pattern_arg->count == 0) {
        (void)fprintf(stderr, "pgrep: missing required operand\n");
        return 2;
    }

    std::string const pattern = pattern_arg->sval[0];
    bool const list_name = list_name_opt->count > 0;
    bool const list_full = list_full_opt->count > 0;
    bool const full_cmd = full_opt->count > 0;
    bool const ignore_case = ignore_case_opt->count > 0;
    bool const invert = invert_opt->count > 0;
    bool const exact = exact_opt->count > 0;
    bool const count_only = count_opt->count > 0;

    bool has_user_filter = false;
    uid_t filter_uid = 0;
    if (user_opt->count > 0) {
        std::string const user_filter = user_opt->sval[0];
        const struct passwd* pw = getpwnam(user_filter.c_str());
        if (pw != nullptr) {
            filter_uid = pw->pw_uid;
        } else {
            char* endptr = nullptr;
            const long val = std::strtol(user_filter.c_str(), &endptr, 10);
            if (endptr != user_filter.c_str() && *endptr == '\0' && val >= 0) {
                filter_uid = static_cast<uid_t>(val);
            } else {
                (void)fprintf(stderr, "pgrep: invalid user '%s'\n", user_filter.c_str());
                return 2;
            }
        }
        has_user_filter = true;
    }

    std::string const pattern_lc = ignore_case ? to_lower(pattern) : "";
    auto matches = [&](const ProcEntry& p) {
        if (has_user_filter && p.uid != filter_uid) {
            return false;
        }
        std::string const target = match_target(p, full_cmd);
        if (target.empty()) {
            return false;
        }
        std::string const tgt = ignore_case ? to_lower(target) : target;
        std::string const pat = ignore_case ? pattern_lc : pattern;
        bool const hit = exact ? (tgt == pat)
                               : (tgt.find(pat) != std::string::npos);
        return invert ? !hit : hit;
    };

    std::vector<ProcEntry> hits;
    for (const ProcEntry& p : enumerate_processes()) {
        if (matches(p)) {
            hits.push_back(p);
        }
    }

    if (count_only) {
        printf("%zu\n", hits.size());
    } else {
        for (const ProcEntry& p : hits) {
            if (list_full) {
                printf("%d %s\n", p.pid, p.cmdline[0] != '\0' ? p.cmdline : p.comm);
            } else if (list_name) {
                printf("%d %s\n", p.pid, p.comm);
            } else {
                printf("%d\n", p.pid);
            }
        }
    }

    return hits.empty() ? 1 : 0;
}

REGISTER_COMMAND("pgrep", pgrep_command, "List process IDs");
