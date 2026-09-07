#include <argtable3.h>
#include <cstdio>
#include <cstring>
#include <selinux/selinux.h>
#include <strings.h>

#include "commands/setenforce.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_usage(const char* prog) {
    printf("Usage: %s [ Enforcing | Permissive | 1 | 0 ]\n", prog);
    printf("  or:  %s [OPTION]\n", prog);
    printf("Set the SELinux enforcement mode.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int setenforce_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_str* mode_opt = arg_str0(NULL, NULL, "Enforcing|Permissive|1|0",
            "SELinux enforcement mode");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, mode_opt, end});

    int const nerrors = at.parse(argc, argv);

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("setenforce");
        return 0;
    }

    if (mode_opt->count == 0) {
        (void)fprintf(stderr, "usage: %s [ Enforcing | Permissive | 1 | 0 ]\n", argv[0]);
        return 1;
    }

    const char* mode = mode_opt->sval[0];
    int enforcing = -1;

    if (strcmp(mode, "1") == 0 || strcasecmp(mode, "Enforcing") == 0) {
        enforcing = 1;
    } else if (strcmp(mode, "0") == 0 || strcasecmp(mode, "Permissive") == 0) {
        enforcing = 0;
    }

    if (enforcing < 0) {
        (void)fprintf(stderr, "usage: %s [ Enforcing | Permissive | 1 | 0 ]\n", argv[0]);
        return 1;
    }

    if (is_selinux_enabled() == 0) {
        (void)fprintf(stderr, "setenforce: SELinux is disabled\n");
        return 1;
    }

    if (security_setenforce(enforcing) < 0) {
        (void)fprintf(stderr, "setenforce: security_setenforce() failed: %s\n",
                strerror(errno));
        return 1;
    }

    return 0;
}

REGISTER_COMMAND("setenforce", setenforce_command, "Set the SELinux enforcement mode");
