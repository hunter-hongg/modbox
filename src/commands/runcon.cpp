#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <argtable3.h>

#include "commands/runcon.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

int runcon_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* compute_opt = arg_lit0("c", "compute", "compute process transition context before running");
    struct arg_str* type_opt = arg_str0("t", "type", "TYPE", "security context type");
    struct arg_str* user_opt = arg_str0("u", "user", "USER", "security context user");
    struct arg_str* role_opt = arg_str0("r", "role", "ROLE", "security context role");
    struct arg_str* range_opt = arg_str0("l", "range", "RANGE", "security context range");
    struct arg_end* end = arg_end(20);
    ArgTable at({help_opt, compute_opt, type_opt, user_opt, role_opt, range_opt, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... COMMAND [ARGS]...\n", argv[0]);
        printf("Run a command with a specified security context.\n");
        printf("\n");
        printf("  -c, --compute      compute process transition context before running\n");
        printf("  -t, --type=TYPE    security context type\n");
        printf("  -u, --user=USER    security context user\n");
        printf("  -r, --role=ROLE    security context role\n");
        printf("  -l, --range=RANGE  security context range\n");
        printf("  -h, --help         display this help and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    int nonopt = argc - 1;
    while (nonopt > 0 && argv[nonopt][0] != '-') nonopt--;

    if (compute_opt->count > 1) {
        fprintf(stderr, "runcon: too many --compute options\n");
        return 0;
    }

    char* con_str = nullptr;
    if (getcon(&con_str) < 0) {
        fprintf(stderr, "runcon: cannot get current context\n");
        return 0;
    }

    context_t con = context_new(con_str);
    freecon(con_str);

    if (!con) {
        fprintf(stderr, "runcon: failed to create context object\n");
        return 0;
    }

    if (type_opt->count > 0) {
        if (context_type_set(con, type_opt->sval[0]) != 0) {
            fprintf(stderr, "runcon: failed to set type '%s'\n", type_opt->sval[0]);
            context_free(con);
            return 0;
        }
    }

    if (user_opt->count > 0) {
        if (context_user_set(con, user_opt->sval[0]) != 0) {
            fprintf(stderr, "runcon: failed to set user '%s'\n", user_opt->sval[0]);
            context_free(con);
            return 0;
        }
    }

    if (role_opt->count > 0) {
        if (context_role_set(con, role_opt->sval[0]) != 0) {
            fprintf(stderr, "runcon: failed to set role '%s'\n", role_opt->sval[0]);
            context_free(con);
            return 0;
        }
    }

    if (range_opt->count > 0) {
        if (context_range_set(con, range_opt->sval[0]) != 0) {
            fprintf(stderr, "runcon: failed to set range '%s'\n", range_opt->sval[0]);
            context_free(con);
            return 0;
        }
    }

    const char* new_ctx = context_str(con);
    if (!new_ctx) {
        fprintf(stderr, "runcon: failed to get new context string\n");
        context_free(con);
        return 0;
    }

    if (compute_opt->count > 0) {
        char* computed = nullptr;
        if (selinux_trans_to_raw_context(new_ctx, &computed) < 0) {
            fprintf(stderr, "runcon: failed to compute transition context\n");
            context_free(con);
            return 0;
        }
        if (setexeccon(computed) < 0) {
            fprintf(stderr, "runcon: failed to set execution context\n");
            freecon(computed);
            context_free(con);
            return 0;
        }
        freecon(computed);
    } else {
        if (setexeccon(new_ctx) < 0) {
            fprintf(stderr, "runcon: failed to set execution context\n");
            context_free(con);
            return 0;
        }
    }

    context_free(con);

    int cmd_argc = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            cmd_argc = argc - i;
            execvp(argv[i], argv + i);
            fprintf(stderr, "runcon: failed to execute '%s': %s\n", argv[i], strerror(errno));
            return 0;
        }
    }

    fprintf(stderr, "runcon: no command specified\n");
    return 0;
}

REGISTER_COMMAND("runcon", runcon_command, "Run command with specified security context");