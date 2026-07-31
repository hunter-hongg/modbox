#include <cstdio>
#include <cstring>
#include <selinux/selinux.h>
#include <argtable3.h>

#include "commands/getenforce.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

int getenforce_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, end});

    int nerrors = at.parse(argc, argv);

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        printf("Usage: %s\n", argv[0]);
        printf("  or:  %s OPTION\n", argv[0]);
        printf("Print the current SELinux mode.\n");
        printf("\n");
        printf("      --help     display this help and exit\n");
        printf("      --version  output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("getenforce");
        return 0;
    }

    // Reject any extra arguments
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "getenforce: unrecognized option '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        } else {
            fprintf(stderr, "getenforce: cannot specify positional argument '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    int enforce = security_getenforce();
    if (enforce < 0) {
        printf("Disabled\n");
    } else if (enforce > 0) {
        printf("Enforcing\n");
    } else {
        printf("Permissive\n");
    }
    return 0;
}

REGISTER_COMMAND("getenforce", getenforce_command, "Print the current SELinux mode");
