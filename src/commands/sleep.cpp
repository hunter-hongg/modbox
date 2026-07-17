#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "commands/sleep.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s NUMBER[SUFFIX]...\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Pause for NUMBER seconds.  SUFFIX may be 's' for seconds (the default),\n");
    printf("'m' for minutes, 'h' for hours, or 'd' for days.\n");
    printf("NUMBER may be an integer or floating-point number.\n");
    printf("Multiple arguments may be given, in which case the total sleep time\n");
    printf("is the sum of the durations specified.\n");
    printf("\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -V, --version  output version information and exit\n");
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
        // Ensure no trailing characters after the suffix
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

static void do_sleep(double seconds) {
    if (seconds <= 0) {
        return;
    }

    // Split into whole seconds and nanoseconds
    time_t sec_part = (time_t)seconds;
    long nsec_part = (long)((seconds - (double)sec_part) * 1000000000.0);

    struct timespec ts;
    ts.tv_sec = sec_part;
    ts.tv_nsec = nsec_part;

    // Retry on EINTR (signal interruption)
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        continue;
    }
}

void sleep_command(int argc, char** argv) {
    const char* prog = argv[0];

    int first_arg = 1;

    // Check for --help or --version in the first argument only
    if (argc > 1) {
        const char* a = argv[1];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(a, "-V") == 0 || strcmp(a, "--version") == 0) {
            printf("sleep (modbox) 1.0\n");
            return;
        }
        if (strcmp(a, "--") == 0) {
            first_arg = 2;
        }
    }

    if (first_arg >= argc) {
        fprintf(stderr, "sleep: missing operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return;
    }

    double total = 0.0;
    for (int i = first_arg; i < argc; i++) {
        const char* a = argv[i];
        double dur = 0.0;
        if (!parse_duration(a, &dur)) {
            fprintf(stderr, "sleep: invalid time interval '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", prog);
            return;
        }
        total += dur;
    }

    do_sleep(total);
}