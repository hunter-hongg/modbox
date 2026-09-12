#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pwd.h>
#include <sys/resource.h>
#include <unistd.h>

#include "commands/renice.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// The three ways a target identifier can be interpreted (util-linux calls
// these "which" selectors). PID is the default.
enum class Target { Pid, Pgrp, User };

// Human-readable form used in both error messages and the success report.
const char* target_label(Target t) {
    switch (t) {
        case Target::Pgrp:
            return "process group ID";
        case Target::User:
            return "user ID";
        case Target::Pid:
        default:
            return "process ID";
    }
}

// The PRIO_* class passed to getpriority/setpriority for each target kind.
int target_which(Target t) {
    switch (t) {
        case Target::Pgrp:
            return PRIO_PGRP;
        case Target::User:
            return PRIO_USER;
        case Target::Pid:
        default:
            return PRIO_PROCESS;
    }
}

void print_help(const char* prog) {
    printf("Usage: %s [options] priority [-g|-p|-u] identifier...\n", prog);
    printf("Alter the priority of running processes.\n");
    printf("\n");
    printf("  -n, --priority <num>   specify the 'absolute' nice value,\n");
    printf("                           but 'relative' when POSIXLY_CORRECT is set\n");
    printf("      --relative <num>   specify the 'relative' nice value\n");
    printf("  -p, --pid              interpret arguments as process ID (default)\n");
    printf("  -g, --pgrp             interpret arguments as process group ID\n");
    printf("  -u, --user             interpret arguments as username or user ID\n");
    printf("  -h, --help             display this help and exit\n");
    printf("  -V, --version          display version information and exit\n");
}

int usage_error(const char* prog) {
    (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    return 1;
}

// Parse a whole-token integer. Returns false when the token is empty or has
// trailing garbage. Mirrors the strictness used by the rest of the repo.
bool parse_long(const char* s, long* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    char* endp = nullptr;
    errno = 0;
    long const val = strtol(s, &endp, 10);
    if (endp == s || *endp != '\0') {
        return false;
    }
    *out = val;
    return true;
}

// Read the current niceness for a target, distinguishing a genuine -1 from
// the -1 error sentinel via errno (same guard as nice.cpp).
bool get_niceness(int which, int id, int* out) {
    errno = 0;
    int const val = getpriority(which, id);
    if (val == -1 && errno != 0) {
        return false;
    }
    *out = val;
    return true;
}

// Resolve a -u identifier to a numeric UID. Accepts a numeric string or a
// user name looked up in the password database.
bool resolve_user(const char* arg, int* uid) {
    long numeric = 0;
    if (parse_long(arg, &numeric)) {
        *uid = static_cast<int>(numeric);
        return true;
    }
    struct passwd const* pw = getpwnam(arg);
    if (pw == nullptr) {
        return false;
    }
    *uid = static_cast<int>(pw->pw_uid);
    return true;
}

// The kernel clamps niceness to [-20, 19]. util-linux reports the *clamped*
// value, so mirror that here rather than echoing the raw arithmetic.
constexpr int kMinNiceness = -20;
constexpr int kMaxNiceness = 19;

int clamp_niceness(int value) {
    if (value < kMinNiceness) {
        return kMinNiceness;
    }
    if (value > kMaxNiceness) {
        return kMaxNiceness;
    }
    return value;
}

// Apply the (possibly relative) priority change to one target. Prints the
// util-linux-style report line on success or a diagnostic on stderr.
// Returns true on success.
bool apply_to_target(const char* prog, Target target, int id, bool relative,
                     int priority) {
    int const which = target_which(target);
    int old_niceness = 0;
    if (!get_niceness(which, id, &old_niceness)) {
        (void)fprintf(stderr, "%s: failed to get priority for %d (%s): %s\n", prog,
                      id, target_label(target), strerror(errno));
        return false;
    }

    int const requested = relative ? old_niceness + priority : priority;
    int const new_niceness = clamp_niceness(requested);
    if (setpriority(which, id, new_niceness) != 0) {
        (void)fprintf(stderr, "%s: failed to set priority for %d (%s): %s\n", prog,
                      id, target_label(target), strerror(errno));
        return false;
    }

    printf("%d (%s) old priority %d, new priority %d\n", id, target_label(target),
           old_niceness, new_niceness);
    return true;
}

// Identifier for a non-user target (numeric only).
bool parse_identifier(const char* prog, Target target, const char* arg, int* id) {
    long value = 0;
    if (!parse_long(arg, &value)) {
        (void)fprintf(stderr, "%s: bad %s value: %s\n", prog, target_label(target),
                      arg);
        return false;
    }
    *id = static_cast<int>(value);
    return true;
}

// Outcome of recognising a leading priority option.
struct PriorityOption {
    bool matched = false;  // argv[i] was -n/--priority/--relative
    bool relative = false; // the value should be applied as a delta
    int value = 0;         // the parsed priority/delta
    int consumed = 0;      // argv entries eaten (1 or 2)
    bool error = false;    // a diagnostic was printed; caller should exit
};

// Parse a priority value, reporting on failure. Returns false after printing
// the util-linux-style diagnostic.
bool parse_priority_value(const char* prog, const char* value, int* out) {
    long parsed = 0;
    if (!parse_long(value, &parsed)) {
        (void)fprintf(stderr, "%s: invalid priority '%s'\n", prog, value);
        return false;
    }
    *out = static_cast<int>(parsed);
    return true;
}

// Recognise a leading priority option. Only the separated forms are accepted
// (-n N, --priority N, --relative N), matching util-linux; the attached
// --priority=N form is *not* recognized.
//
// `-n` and `--priority` select the *absolute* value unless POSIXLY_CORRECT is
// set, in which case `-n` becomes relative — but `--priority` always stays
// absolute (matching util-linux). `--relative` is always relative.
PriorityOption parse_priority_option(const char* prog, int argc, char** argv,
                                     int i) {
    PriorityOption result;
    const char* s = argv[i];

    if (strcmp(s, "--priority") == 0) {
        result.matched = true;
        result.relative = false;
    } else if (strcmp(s, "-n") == 0) {
        result.matched = true;
        result.relative = (getenv("POSIXLY_CORRECT") != nullptr);
    } else if (strcmp(s, "--relative") == 0) {
        result.matched = true;
        result.relative = true;
    }

    if (!result.matched) {
        return result;
    }

    if (i + 1 >= argc) {
        (void)fprintf(stderr, "%s: not enough arguments\n", prog);
        result.error = true;
        return result;
    }

    result.consumed = 2;
    if (!parse_priority_value(prog, argv[i + 1], &result.value)) {
        result.error = true;
    }
    return result;
}

}  // namespace

int renice_command(int argc, char** argv) {
    const char* prog = "renice";
    if (argc > 0 && argv[0] != nullptr) {
        prog = argv[0];
    }

    // Scan the leading priority option, if any. `-n`/`--priority` are
    // absolute unless POSIXLY_CORRECT is set (then `-n` is relative);
    // `--relative` is always relative.
    bool relative = false;
    bool have_priority = false;
    int priority = 0;
    int i = 1;

    while (i < argc) {
        const char* s = argv[i];

        if (strcmp(s, "-h") == 0 || strcmp(s, "--help") == 0) {
            print_help(prog);
            return 0;
        }
        if (strcmp(s, "-V") == 0 || strcmp(s, "--version") == 0) {
            print_version("renice");
            return 0;
        }

        PriorityOption const opt = parse_priority_option(prog, argc, argv, i);
        if (!opt.matched) {
            break;
        }
        if (opt.error) {
            return usage_error(prog);
        }
        priority = opt.value;
        relative = opt.relative;
        have_priority = true;
        i += opt.consumed;
    }

    // A bare first argument (no -n/--priority/--relative) is the priority.
    if (!have_priority) {
        if (i >= argc) {
            (void)fprintf(stderr, "%s: not enough arguments\n", prog);
            return usage_error(prog);
        }
        if (!parse_priority_value(prog, argv[i], &priority)) {
            return usage_error(prog);
        }
        i++;
    }

    // Optional target selector; PID is the default.
    Target target = Target::Pid;
    if (i < argc) {
        const char* s = argv[i];
        if (strcmp(s, "-p") == 0 || strcmp(s, "--pid") == 0) {
            target = Target::Pid;
            i++;
        } else if (strcmp(s, "-g") == 0 || strcmp(s, "--pgrp") == 0) {
            target = Target::Pgrp;
            i++;
        } else if (strcmp(s, "-u") == 0 || strcmp(s, "--user") == 0) {
            target = Target::User;
            i++;
        }
    }

    // Skip an options terminator so identifiers beginning with '-' are not
    // mistaken for options.
    if (i < argc && strcmp(argv[i], "--") == 0) {
        i++;
    }

    if (i >= argc) {
        (void)fprintf(stderr, "%s: not enough arguments\n", prog);
        return usage_error(prog);
    }

    bool ok = true;
    for (; i < argc; i++) {
        const char* arg = argv[i];
        int id = 0;
        if (target == Target::User) {
            if (!resolve_user(arg, &id)) {
                (void)fprintf(stderr, "%s: unknown user %s\n", prog, arg);
                ok = false;
                continue;
            }
        } else if (!parse_identifier(prog, target, arg, &id)) {
            ok = false;
            continue;
        }

        if (!apply_to_target(prog, target, id, relative, priority)) {
            ok = false;
        }
    }

    return ok ? 0 : 1;
}
REGISTER_COMMAND("renice", renice_command, "Alter priority of running processes");
