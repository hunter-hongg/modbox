#include <cstdio>
#include <cstring>
#include <selinux/selinux.h>
#include <argtable3.h>

#include "commands/getenforce.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static int print_arg_errors(struct arg_end* end, const char* prog) {
    for (int i = 0; i < end->count; i++) {
        const char* argval = end->argval[i] ? end->argval[i] : "";
        if (end->error[i] == ARG_ELONGOPT) {
            fprintf(stderr, "%s: unrecognized option '%s'\n", prog, argval);
        } else {
            fprintf(stderr, "%s: unexpected argument '%s'\n", prog, argval);
        }
    }
    fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    return 1;
}

int getenforce_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, end});

    int nerrors = at.parse(argc, argv);

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
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
