#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/resource.h>
#include <unistd.h>

#include "commands/nice.hpp"
#include "commands/command_macros.hpp"

// EXIT_CANCELED: nice itself failed (bad option, cannot get/set niceness).
static const int EXIT_CANCELED = 125;
static const int EXIT_CANNOT_INVOKE = 126;
static const int EXIT_ENOENT = 127;

// The "zero" niceness offset; adjustments are clamped to [-2*NZERO, 2*NZERO].
static const long NZERO = 20;

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION] [COMMAND [ARG]...]\n", prog);
    printf("Run COMMAND with an adjusted niceness, which affects process scheduling.\n");
    printf("With no COMMAND, print the current niceness.  Niceness values range from\n");
    printf("-20 (most favorable to the process) to 19 (least favorable to the process).\n");
    printf("\n");
    printf("  -n, --adjustment=N   add integer N to the niceness (default 10)\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

static void usage_error(const char* prog) {
    fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    exit(EXIT_CANCELED);
}

// Parse an integer adjustment, clamp it to [-2*NZERO, 2*NZERO] and accumulate
// it into *adjustment. Returns false on a malformed value.
static bool accumulate_adjustment(const char* prog, const char* s, long* adjustment) {
    if (s == nullptr || *s == '\0') {
        return false;
    }

    char* endp = nullptr;
    errno = 0;
    long val = strtol(s, &endp, 10);
    if (endp == s || *endp != '\0') {
        return false;
    }

    if (val > 2 * NZERO) {
        val = 2 * NZERO;
    } else if (val < -2 * NZERO) {
        val = -2 * NZERO;
    }

    *adjustment += val;
    (void)prog;
    return true;
}

void nice_command(int argc, char** argv) {
    const char* prog = argv[0];

    long adjustment = 0;
    bool have_adjustment = false;
    int i = 1;

    while (i < argc) {
        const char* s = argv[i];

        // Obsolete form: -N, --N, -+N (e.g. -10, --5, -+5).
        if (s[0] == '-' && s[1] != '\0') {
            int idx = (s[1] == '-' || s[1] == '+') ? 2 : 1;
            if (isdigit((unsigned char)s[idx])) {
                if (!accumulate_adjustment(prog, s + 1, &adjustment)) {
                    fprintf(stderr, "%s: invalid adjustment '%s'\n", prog, s + 1);
                    usage_error(prog);
                }
                have_adjustment = true;
                i++;
                continue;
            }
        }

        if (strcmp(s, "--") == 0) {
            i++;
            break;
        }
        if (strcmp(s, "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(s, "--version") == 0) {
            printf("nice (modbox) 1.0\n");
            return;
        }

        // -n N  and  -nN
        if (strcmp(s, "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option requires an argument -- 'n'\n", prog);
                usage_error(prog);
            }
            if (!accumulate_adjustment(prog, argv[i + 1], &adjustment)) {
                fprintf(stderr, "%s: invalid adjustment '%s'\n", prog, argv[i + 1]);
                usage_error(prog);
            }
            have_adjustment = true;
            i += 2;
            continue;
        }
        if (strncmp(s, "-n", 2) == 0) {
            if (!accumulate_adjustment(prog, s + 2, &adjustment)) {
                fprintf(stderr, "%s: invalid adjustment '%s'\n", prog, s + 2);
                usage_error(prog);
            }
            have_adjustment = true;
            i++;
            continue;
        }

        // --adjustment=N  and  --adjustment N
        if (strncmp(s, "--adjustment=", 13) == 0) {
            if (!accumulate_adjustment(prog, s + 13, &adjustment)) {
                fprintf(stderr, "%s: invalid adjustment '%s'\n", prog, s + 13);
                usage_error(prog);
            }
            have_adjustment = true;
            i++;
            continue;
        }
        if (strcmp(s, "--adjustment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option '--adjustment' requires an argument\n", prog);
                usage_error(prog);
            }
            if (!accumulate_adjustment(prog, argv[i + 1], &adjustment)) {
                fprintf(stderr, "%s: invalid adjustment '%s'\n", prog, argv[i + 1]);
                usage_error(prog);
            }
            have_adjustment = true;
            i += 2;
            continue;
        }

        // Any other leading-dash token is an unknown option.
        if (s[0] == '-' && s[1] != '\0') {
            fprintf(stderr, "%s: unrecognized option '%s'\n", prog, s);
            usage_error(prog);
        }

        // First non-option argument: the command begins here.
        break;
    }

    // No command given.
    if (i >= argc) {
        if (have_adjustment) {
            fprintf(stderr, "%s: a command must be given with an adjustment\n", prog);
            usage_error(prog);
        }
        errno = 0;
        int current = getpriority(PRIO_PROCESS, 0);
        if (current == -1 && errno != 0) {
            fprintf(stderr, "%s: cannot get niceness: %s\n", prog, strerror(errno));
            exit(EXIT_CANCELED);
        }
        printf("%d\n", current);
        return;
    }

    if (!have_adjustment) {
        adjustment = 10;
    }

    errno = 0;
    int current = getpriority(PRIO_PROCESS, 0);
    if (current == -1 && errno != 0) {
        fprintf(stderr, "%s: cannot get niceness: %s\n", prog, strerror(errno));
        exit(EXIT_CANCELED);
    }

    if (setpriority(PRIO_PROCESS, 0, current + adjustment) != 0) {
        // Failing to set the niceness is non-fatal: warn and still run COMMAND.
        fprintf(stderr, "%s: cannot set niceness: %s\n", prog, strerror(errno));
    }

    execvp(argv[i], &argv[i]);

    int code = (errno == ENOENT) ? EXIT_ENOENT : EXIT_CANNOT_INVOKE;
    fprintf(stderr, "%s: %s: %s\n", prog, argv[i], strerror(errno));
    exit(code);
}
REGISTER_COMMAND("nice", nice_command, "Run with modified scheduling priority");
