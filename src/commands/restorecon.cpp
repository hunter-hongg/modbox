#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "commands/restorecon.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include <selinux/selinux.h>
#include <selinux/restorecon.h>

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTION]... FILE...\n", prog);
    printf("Restore the default SELinux security context of each FILE.\n");
    printf("\n");
    printf("  -R, --recursive      operate on files and directories recursively\n");
    printf("  -v, --verbose        output a diagnostic for every file processed\n");
    printf("  -n, --nochange       don't change any file labels\n");
    printf("  -F, --force          force reset of customizable and default contexts\n");
    printf("  -i, --ignore-errors  continue on errors\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
}

// selinux_restorecon handles the recursion internally when
// SELINUX_RESTORECON_RECURSE is set, and emits a single "Warning no default
// label" for each top-level path that lacks a match — matching the reference.
// Calling it once per top-level path (instead of one call per child via nftw)
// is what makes the verbose/warning output line up with system restorecon.
static int restorecon_one(const char* path, unsigned int flags, int ignore_errors) {
    int const rc = selinux_restorecon(path, flags);
    if (rc != 0 && ignore_errors == 0) {
        (void)fprintf(stderr, "restorecon: %s: failed to relabel\n", path);
    }
    return rc;
}

int restorecon_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* recursive_opt = arg_lit0("R", "recursive", "operate on files and directories recursively");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "output a diagnostic for every file processed");
    struct arg_lit* nochange_opt = arg_lit0("n", "nochange", "don't change any file labels");
    struct arg_lit* force_opt = arg_lit0("F", "force", "force reset of customizable and default contexts");
    struct arg_lit* ignore_opt = arg_lit0("i", "ignore-errors", "continue on errors");
    struct arg_file* files_opt = arg_file0(NULL, NULL, "FILE", "files to relabel");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, recursive_opt, verbose_opt,
                 nochange_opt, force_opt, ignore_opt, files_opt, end});

    int const nerrors = at.parse(argc, argv);
    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("restorecon");
        return 0;
    }

    if (files_opt->count == 0) {
        (void)fprintf(stderr, "%s: missing operand\n", argv[0]);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    unsigned int flags = SELINUX_RESTORECON_REALPATH;
    if (recursive_opt->count > 0) { flags |= SELINUX_RESTORECON_RECURSE; }
    if (verbose_opt->count > 0)   { flags |= SELINUX_RESTORECON_VERBOSE; }
    if (nochange_opt->count > 0)  { flags |= SELINUX_RESTORECON_NOCHANGE; }
    if (force_opt->count > 0)     { flags |= SELINUX_RESTORECON_SET_SPECFILE_CTX; }
    if (ignore_opt->count > 0)    { flags |= SELINUX_RESTORECON_COUNT_ERRORS; }

    int const ignore_errors = ignore_opt->count > 0 ? 1 : 0;
    int rc = 0;
    for (int i = 0; i < files_opt->count; i++) {
        if (restorecon_one(files_opt->filename[i], flags, ignore_errors) != 0) {
            rc = 1;
        }
    }

    return rc;
}

REGISTER_COMMAND("restorecon", restorecon_command, "Restore SELinux security context of files");
