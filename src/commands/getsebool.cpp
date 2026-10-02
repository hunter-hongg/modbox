#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <selinux/selinux.h>

#include "commands/getsebool.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

// Print one boolean's active/pending state, using the caller's already-queried
// active value so the boolean is enumerated once. Returns 0; a failed lookup is
// reported by the caller, matching the reference's exit 255.
static int print_one_boolean(const char* name, int active) {
    int const pending = security_get_boolean_pending(name);
    if (pending < 0) {
        printf("%s --> %s\n", name, active != 0 ? "on" : "off");
        return 0;
    }
    if (active == pending) {
        printf("%s --> %s\n", name, active != 0 ? "on" : "off");
    } else {
        printf("%s --> %s %s\n", name,
               active != 0 ? "on" : "off",
               pending != 0 ? "on" : "off");
    }
    return 0;
}

static void print_usage(const char* prog) {
    fprintf(stderr, "usage:  %s -a or %s boolean...\n", prog, prog);
}

// selinux_boolean_t's error path prints "Error getting active value for NAME"
// to stderr and returns -1, which the reference surfaces as exit 255.
static int report_query_failure(const char* name) {
    (void)fprintf(stderr, "Error getting active value for %s\n", name);
    return 255;
}

int getsebool_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* all_opt = arg_lit0("a", "all", "report the state of all booleans");
    struct arg_file* bools_opt = arg_filen(NULL, NULL, "BOOLEAN", 0, 1000, "booleans to query");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, all_opt, bools_opt, end});

    int const nerrors = at.parse(argc, argv);
    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        // The reference has no long options: getopt rejects "--help" as an
        // unknown option and prints the usage line, exiting 1. modbox keeps
        // --help/--version as its standard affordances instead. Likewise the
        // reference treats a bare "--" as a boolean name and exits 255 with
        // "Error getting active value for --", while argtable3 consumes it
        // as the end-of-options marker.
        printf("Usage: %s [-a] [BOOLEAN]...\n", argv[0]);
        printf("Report whether SELinux boolean BOOLEAN is on or off.\n");
        printf("\n");
        printf("  -a, --all           report the state of all booleans\n");
        printf("      --help           display this help and exit\n");
        printf("      --version        output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("getsebool");
        return 0;
    }

    // The reference's usage line names exactly these two forms and nothing
    // else, so any other combination is a usage error. Repeated -a flags are
    // accepted (the reference takes "-aa" as all-booleans too).
    bool const all_form = all_opt->count > 0 && bools_opt->count == 0;
    bool const names_form = all_opt->count == 0 && bools_opt->count > 0;
    if (!all_form && !names_form) {
        print_usage(argv[0]);
        return 1;
    }

    if (all_opt->count > 0) {
        char** names = nullptr;
        int len = 0;
        if (security_get_boolean_names(&names, &len) < 0) {
            (void)fprintf(stderr,
                          "%s: unable to read SELinux booleans (SELinux disabled or unsupported)\n",
                          argv[0]);
            return 1;
        }
        for (int i = 0; i < len; i++) {
            int const active = security_get_boolean_active(names[i]);
            if (active < 0) {
                continue;
            }
            (void)print_one_boolean(names[i], active);
        }
        for (int i = 0; i < len; i++) {
            free(names[i]);
        }
        free(names);
        return 0;
    }

    // The reference stops at the first boolean it cannot query: with
    // "valid nope valid" it prints only the leading valid entry and the
    // error, never reaching the trailing one.
    int rc = 0;
    for (int i = 0; i < bools_opt->count; i++) {
        int const active = security_get_boolean_active(bools_opt->filename[i]);
        if (active < 0) {
            rc = report_query_failure(bools_opt->filename[i]);
            break;
        }
        (void)print_one_boolean(bools_opt->filename[i], active);
    }
    return rc;
}

REGISTER_COMMAND("getsebool", getsebool_command, "Report SELinux boolean states");
