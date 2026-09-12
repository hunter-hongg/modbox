#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/chrt.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// Exit codes follow the repository convention: 0 success (including queries,
// -m, help and version), 1 runtime failure, 2 usage/parse error — the same
// split used by ping/arping/dig/jq and their tests.
constexpr int kExitOk = 0;
constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

// `execvp` convention for a command that cannot be found.
constexpr int kExitNoEnt = 127;

// A schedulable policy, its command-line spellings, and the kernel constant.
// This table is the single source of truth consumed by option parsing, the
// help text and the -m listing, so the three can never drift apart.
struct PolicyInfo {
    const char* short_opt;  // "-f"
    const char* long_opt;   // "--fifo"
    const char* name;       // "SCHED_FIFO"
    int value;              // SCHED_FIFO
    bool needs_priority;    // FIFO/RR reject a missing priority argument
};

// SCHED_DEADLINE and SCHED_EXT are absent on older libc headers, so guard them
// and advertise only what this build can actually name.
const PolicyInfo kPolicies[] = {
    {.short_opt = "-o", .long_opt = "--other", .name = "SCHED_OTHER",
     .value = SCHED_OTHER, .needs_priority = false},
    {.short_opt = "-b", .long_opt = "--batch", .name = "SCHED_BATCH",
     .value = SCHED_BATCH, .needs_priority = false},
    {.short_opt = "-i", .long_opt = "--idle", .name = "SCHED_IDLE",
     .value = SCHED_IDLE, .needs_priority = false},
    {.short_opt = "-f", .long_opt = "--fifo", .name = "SCHED_FIFO",
     .value = SCHED_FIFO, .needs_priority = true},
    {.short_opt = "-r", .long_opt = "--rr", .name = "SCHED_RR",
     .value = SCHED_RR, .needs_priority = true},
#ifdef SCHED_DEADLINE
    {.short_opt = "-d", .long_opt = "--deadline", .name = "SCHED_DEADLINE",
     .value = SCHED_DEADLINE, .needs_priority = false},
#endif
#ifdef SCHED_EXT
    {.short_opt = "-e", .long_opt = "--ext", .name = "SCHED_EXT",
     .value = SCHED_EXT, .needs_priority = false},
#endif
};

// Return the policy whose -x/--long spelling matches `arg`, or nullptr.
const PolicyInfo* find_policy_by_flag(const char* arg) {
    for (const PolicyInfo& policy : kPolicies) {
        if (strcmp(arg, policy.short_opt) == 0 ||
            strcmp(arg, policy.long_opt) == 0) {
            return &policy;
        }
    }
    return nullptr;
}

// Return the policy whose SCHED_* name corresponds to kernel value `value`.
const PolicyInfo* find_policy_by_value(int value) {
    for (const PolicyInfo& policy : kPolicies) {
        if (policy.value == value) {
            return &policy;
        }
    }
    return nullptr;
}

// Render a policy reported by the kernel. A value this build does not know is
// described numerically rather than mislabelled as some other policy.
const char* policy_name(int value, char* fallback, size_t fallback_size) {
    const PolicyInfo* info = find_policy_by_value(value);
    if (info != nullptr) {
        return info->name;
    }
    (void)snprintf(fallback, fallback_size, "SCHED_%d", value);
    return fallback;
}

// Flags that take no argument, their spellings, and their help text. This is
// the single source of truth for the help listing, so it cannot drift from the
// options the parser actually accepts.
struct FlagInfo {
    const char* short_opt;  // "-p"
    const char* long_opt;   // "--pid"
    const char* help;       // printed verbatim in --help
};

const FlagInfo kFlags[] = {
    {.short_opt = "-a",
     .long_opt = "--all-tasks",
     .help = "operate on all the tasks (threads) for a given pid"},
    {.short_opt = "-m",
     .long_opt = "--max",
     .help = "show min and max valid priorities"},
    {.short_opt = "-p",
     .long_opt = "--pid",
     .help = "operate on existing given pid"},
    {.short_opt = "-R",
     .long_opt = "--reset-on-fork",
     .help = "set reset-on-fork flag"},
    {.short_opt = "-v",
     .long_opt = "--verbose",
     .help = "display status information"},
};

void print_help(const char* prog) {
    printf("Usage: %s [options] [<priority>] <command> [<argument>...]\n", prog);
    printf("   or: %s -p [options] [<priority>] <PID>\n", prog);
    printf("   or: %s -p [options] <PID>\n", prog);
    printf("Show or change the real-time scheduling attributes of a process.\n");
    printf("\n");
    printf("Set policy:\n");
    printf(" %s [options] [<priority>] <command> [<argument>...]\n", prog);
    printf(" %s --pid <policy-option> [options] [<priority>] <PID>\n", prog);
    printf("\n");
    printf("Show policy:\n");
    printf(" %s --pid <PID>\n", prog);
    printf("\n");
    printf("Policy options:\n");
    for (const PolicyInfo& policy : kPolicies) {
        printf(" %s, %-12s set policy to %s\n", policy.short_opt,
               policy.long_opt, policy.name);
    }
    printf("\n");
    printf("Other options:\n");
    for (const FlagInfo& flag : kFlags) {
        char opts_buf[32];
        (void)snprintf(opts_buf, sizeof(opts_buf), "%s, %s", flag.short_opt,
                       flag.long_opt);
        printf(" %-20s %s\n", opts_buf, flag.help);
    }
    printf("\n");
    printf(" -h, --help           display this help and exit\n");
    printf(" -V, --version        display version information and exit\n");
}

// The standard modbox usage-error hint, printed by every usage failure.
void print_help_hint(const char* prog) {
    (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
}

// Parse a whole-token integer, rejecting empty input and trailing garbage.
bool parse_int(const char* s, long* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    char* endp = nullptr;
    errno = 0;
    long const value = strtol(s, &endp, 10);
    if (endp == s || *endp != '\0') {
        return false;
    }
    *out = value;
    return true;
}

// Options gathered by the parser, shared by the query and set paths.
struct Options {
    bool pid_form = false;               // -p/--pid given
    bool verbose = false;                // -v
    bool reset_on_fork = false;          // -R
    bool show_max = false;               // -m
    const PolicyInfo* policy = nullptr;  // the selected policy flag
};

// Recognised flags that take no argument. Returns true when `arg` was one of
// them; help/version are signalled separately because they terminate.
bool consume_plain_option(const char* arg, Options* opts) {
    if (strcmp(arg, "-p") == 0 || strcmp(arg, "--pid") == 0) {
        opts->pid_form = true;
        return true;
    }
    if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
        opts->verbose = true;
        return true;
    }
    if (strcmp(arg, "-R") == 0 || strcmp(arg, "--reset-on-fork") == 0) {
        opts->reset_on_fork = true;
        return true;
    }
    if (strcmp(arg, "-m") == 0 || strcmp(arg, "--max") == 0) {
        opts->show_max = true;
        return true;
    }
    // -a/--all-tasks is accepted for compatibility with util-linux scripts. We
    // operate on the process as a whole, so there is no per-thread walk.
    return strcmp(arg, "-a") == 0 || strcmp(arg, "--all-tasks") == 0;
}

// Print the valid priority range for every policy (chrt -m). The order mirrors
// upstream util-linux, which lists the real-time policies first.
void print_max() {
    static const int kOrder[] = {
        SCHED_OTHER, SCHED_FIFO, SCHED_RR, SCHED_BATCH, SCHED_IDLE,
#ifdef SCHED_DEADLINE
        SCHED_DEADLINE,
#endif
#ifdef SCHED_EXT
        SCHED_EXT,
#endif
    };
    for (int const value : kOrder) {
        const PolicyInfo* policy = find_policy_by_value(value);
        if (policy == nullptr) {
            continue;
        }
        int const lo = sched_get_priority_min(policy->value);
        int const hi = sched_get_priority_max(policy->value);
        if (lo < 0 || hi < 0) {
            continue;
        }
        printf("%s min/max priority\t: %d/%d\n", policy->name, lo, hi);
    }
}

// Report the current policy and priority of `pid`, one fact per line, matching
// the wording of upstream chrt.
void print_status(pid_t pid, int policy) {
    char fallback[32];
    printf("pid %d's current scheduling policy: %s\n", static_cast<int>(pid),
           policy_name(policy, fallback, sizeof(fallback)));
}

// Query one PID. Returns true on success.
bool query_pid(const char* prog, pid_t pid, bool verbose) {
    int const policy = sched_getscheduler(pid);
    if (policy < 0) {
        (void)fprintf(stderr, "%s: failed to get pid %d's policy: %s\n", prog,
                      static_cast<int>(pid), strerror(errno));
        return false;
    }

    struct sched_param param {};
    if (sched_getparam(pid, &param) != 0) {
        (void)fprintf(stderr, "%s: failed to get pid %d's policy: %s\n", prog,
                      static_cast<int>(pid), strerror(errno));
        return false;
    }

    print_status(pid, policy);
    printf("pid %d's current scheduling priority: %d\n", static_cast<int>(pid),
           param.sched_priority);
    if (verbose) {
        int const lo = sched_get_priority_min(policy);
        int const hi = sched_get_priority_max(policy);
        if (lo >= 0 && hi >= 0) {
            printf("pid %d's scheduling priority range: %d - %d\n",
                   static_cast<int>(pid), lo, hi);
        }
    }
    return true;
}

// Build the sched_param for a request. Returns false when the policy needs a
// priority that was not supplied.
bool build_param(const PolicyInfo* policy, bool have_priority, long priority,
                 struct sched_param* param) {
    param->sched_priority = 0;
    if (!have_priority) {
        return !policy->needs_priority;
    }
    if (policy->needs_priority || priority != 0) {
        param->sched_priority = static_cast<int>(priority);
    }
    return true;
}

// Report the "policy needs a priority" misuse. Upstream prints no --help hint
// for this particular diagnostic, so neither do we.
void report_needs_priority(const char* prog, const PolicyInfo* policy) {
    (void)fprintf(stderr, "%s: policy %s requires a priority argument\n", prog,
                  policy->name);
}

// Reject an out-of-range priority with a clear message rather than letting the
// kernel return a bare EINVAL. Returns true when the value is usable.
bool check_priority_range(const char* prog, const PolicyInfo* policy,
                          long priority) {
    int const lo = sched_get_priority_min(policy->value);
    int const hi = sched_get_priority_max(policy->value);
    if (lo < 0 || hi < 0) {
        return true;  // range unknown; let the kernel adjudicate
    }
    if (priority >= lo && priority <= hi) {
        return true;
    }
    (void)fprintf(stderr, "%s: priority %ld out of range for %s (%d - %d)\n",
                  prog, priority, policy->name, lo, hi);
    print_help_hint(prog);
    return false;
}

// Validate the requested policy/priority and apply it to `pid` (0 meaning the
// calling process). Returns 0 on success, or an exit code after reporting the
// failure. Both the set-PID and launch paths share this shape.
int apply_policy(const char* prog, const Options& opts,
                 const PolicyInfo* policy, bool have_priority, long priority,
                 pid_t pid) {
    if (have_priority && !check_priority_range(prog, policy, priority)) {
        return kExitFailure;
    }
    if (!have_priority && policy->needs_priority) {
        report_needs_priority(prog, policy);
        return kExitFailure;
    }

    struct sched_param param {};
    (void)build_param(policy, have_priority, priority, &param);

    int const flags = opts.reset_on_fork ? SCHED_RESET_ON_FORK : 0;
    if (sched_setscheduler(pid, policy->value | flags, &param) != 0) {
        (void)fprintf(stderr, "%s: failed to set pid %d's policy: %s\n", prog,
                      static_cast<int>(pid), strerror(errno));
        return kExitFailure;
    }
    return kExitOk;
}

// Apply a policy/priority to an existing PID (set mode). Returns an exit code.
int set_pid(const char* prog, const Options& opts, bool have_priority,
            long priority, pid_t pid) {
    const PolicyInfo* policy = opts.policy;
    if (policy == nullptr) {
        policy = find_policy_by_value(sched_getscheduler(pid));
        if (policy == nullptr) {
            (void)fprintf(stderr, "%s: failed to get pid %d's policy: %s\n",
                          prog, static_cast<int>(pid), strerror(errno));
            return kExitFailure;
        }
    }

    int const result =
        apply_policy(prog, opts, policy, have_priority, priority, pid);
    if (result != kExitOk) {
        return result;
    }
    if (opts.verbose) {
        return query_pid(prog, pid, false) ? kExitOk : kExitFailure;
    }
    return kExitOk;
}

// Run `argv[cmd_index...]` under the requested policy/priority (launch mode).
// On success this never returns — the child is exec'd. On failure it returns
// the exit code the child should use.
[[noreturn]] void launch_command(const char* prog, const Options& opts,
                                 bool have_priority, long priority,
                                 int cmd_index, char** argv) {
    const PolicyInfo* policy = opts.policy;
    if (policy == nullptr) {
        (void)fprintf(stderr, "%s: no scheduling policy specified\n", prog);
        print_help_hint(prog);
        _exit(kExitUsage);
    }

    int const result = apply_policy(prog, opts, policy, have_priority, priority,
                                    /*pid=*/0);
    if (result != kExitOk) {
        _exit(result);
    }

    // argv[cmd_index...] is already a contiguous NULL-terminated vector.
    execvp(argv[cmd_index], &argv[cmd_index]);
    (void)fprintf(stderr, "%s: failed to execute %s: %s\n", prog,
                  argv[cmd_index], strerror(errno));
    _exit(errno == ENOENT ? kExitNoEnt : kExitFailure);
}

// Parse the options, leaving `i` at the first positional token. Returns an
// exit code when help/version/parse error terminated the run; sets *done so
// the caller knows to return immediately.
int parse_options(const char* prog, int argc, char** argv, Options* opts,
                  int* i, bool* done) {
    *done = false;
    for (*i = 1; *i < argc; (*i)++) {
        const char* arg = argv[*i];
        if (strcmp(arg, "--") == 0) {
            (*i)++;
            return kExitOk;
        }
        if (arg[0] != '-' || arg[1] == '\0') {
            return kExitOk;  // start of the positional tail
        }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_help(prog);
            *done = true;
            return kExitOk;
        }
        if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            print_version("chrt");
            *done = true;
            return kExitOk;
        }
        if (consume_plain_option(arg, opts)) {
            continue;
        }
        const PolicyInfo* policy = find_policy_by_flag(arg);
        if (policy != nullptr) {
            opts->policy = policy;
            continue;
        }
        // SCHED_DEADLINE parameters: accepted so upstream scripts parse, but
        // modbox does not implement deadline scheduling (see the man page).
        if (strcmp(arg, "-T") == 0 || strcmp(arg, "--sched-runtime") == 0 ||
            strcmp(arg, "-P") == 0 || strcmp(arg, "--sched-period") == 0 ||
            strcmp(arg, "-D") == 0 || strcmp(arg, "--sched-deadline") == 0) {
            if (*i + 1 < argc) {
                (*i)++;
            }
            continue;
        }
        (void)fprintf(stderr, "%s: unrecognized option '%s'\n", prog, arg);
        print_help_hint(prog);
        *done = true;
        return kExitUsage;
    }
    return kExitOk;
}

// Report `too few arguments` plus the help hint.
int too_few_arguments(const char* prog) {
    (void)fprintf(stderr, "%s: too few arguments\n", prog);
    print_help_hint(prog);
    return kExitFailure;
}

// Report a non-numeric PID argument. Upstream prints no --help hint here.
int invalid_pid_argument(const char* prog, const char* arg) {
    (void)fprintf(stderr, "%s: invalid PID argument: '%s'\n", prog, arg);
    return kExitFailure;
}

// Handle the `-p` (existing PID) forms: a bare query, or a set request whose
// arguments are `[<priority>] <PID>`.
int run_pid_form(const char* prog, const Options& opts, int argc, char** argv,
                 int i) {
    if (opts.policy == nullptr) {
        if (i >= argc) {
            return too_few_arguments(prog);
        }
        long pid_value = 0;
        if (!parse_int(argv[i], &pid_value)) {
            return invalid_pid_argument(prog, argv[i]);
        }
        if (i + 1 < argc) {
            return invalid_pid_argument(prog, argv[i + 1]);
        }
        return query_pid(prog, static_cast<pid_t>(pid_value), opts.verbose)
                   ? kExitOk
                   : kExitFailure;
    }

    if (i >= argc) {
        return too_few_arguments(prog);
    }
    long first_value = 0;
    if (!parse_int(argv[i], &first_value)) {
        return invalid_pid_argument(prog, argv[i]);
    }
    // A single number is the PID alone; two numbers are priority then PID.
    // A policy that requires a priority cannot be applied with one number, so
    // report that misuse rather than silently treating the number as a PID.
    if (i + 1 >= argc) {
        if (opts.policy->needs_priority) {
            report_needs_priority(prog, opts.policy);
            return kExitFailure;
        }
        return set_pid(prog, opts, false, 0, static_cast<pid_t>(first_value));
    }
    long pid_value = 0;
    if (!parse_int(argv[i + 1], &pid_value)) {
        return invalid_pid_argument(prog, argv[i + 1]);
    }
    if (i + 2 < argc) {
        return invalid_pid_argument(prog, argv[i + 2]);
    }
    return set_pid(prog, opts, true, first_value, static_cast<pid_t>(pid_value));
}

// Handle the launch form: `[<priority>] <command> [<args>...]`.
int run_launch_form(const char* prog, const Options& opts, int argc, char** argv,
                    int i) {
    // Without an explicit policy flag, upstream defaults to SCHED_RR.
    const PolicyInfo* policy = opts.policy;
    if (policy == nullptr) {
        policy = find_policy_by_value(SCHED_RR);
    }

    if (i >= argc) {
        (void)fprintf(stderr, "%s: no command or priority specified\n", prog);
        print_help_hint(prog);
        return kExitFailure;
    }

    // A leading integer is the priority when a command follows it, or when the
    // policy requires a priority and a token remains to be the command.
    bool have_priority = false;
    long priority = 0;
    if (parse_int(argv[i], &priority) && (i + 1 < argc || policy->needs_priority)) {
        have_priority = true;
        i++;
    }

    if (i >= argc) {
        (void)fprintf(stderr, "%s: no command or priority specified\n", prog);
        print_help_hint(prog);
        return kExitFailure;
    }

    // A policy that requires a priority, given only a command, is a misuse.
    if (!have_priority && policy->needs_priority) {
        report_needs_priority(prog, policy);
        return kExitFailure;
    }

    Options effective = opts;
    effective.policy = policy;

    pid_t const child = fork();
    if (child < 0) {
        (void)fprintf(stderr, "%s: fork: %s\n", prog, strerror(errno));
        return kExitFailure;
    }
    if (child == 0) {
        launch_command(prog, effective, have_priority, priority, i, argv);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            (void)fprintf(stderr, "%s: waitpid: %s\n", prog, strerror(errno));
            return kExitFailure;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return kExitFailure;
}

}  // namespace

int chrt_command(int argc, char** argv) {
    const char* prog = "chrt";
    if (argc > 0 && argv[0] != nullptr) {
        prog = argv[0];
    }

    Options opts;
    int i = 1;
    bool done = false;
    int const parse_rc = parse_options(prog, argc, argv, &opts, &i, &done);
    if (done) {
        return parse_rc;
    }

    if (opts.show_max) {
        print_max();
        return kExitOk;
    }

    return opts.pid_form ? run_pid_form(prog, opts, argc, argv, i)
                         : run_launch_form(prog, opts, argc, argv, i);
}

REGISTER_COMMAND("chrt", chrt_command,
                 "Show or change real-time scheduling attributes of a process");
