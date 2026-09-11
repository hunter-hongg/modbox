#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ftw.h>
#include <sys/stat.h>
#include <selinux/selinux.h>
#include <selinux/restorecon.h>

#include "commands/restorecon.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

struct RestoreconGlob {
    unsigned int flags;
    int errors;
    int ignore;
};

RestoreconGlob restorecon_glob;

} // namespace

static int restorecon_one_file(const char* path, unsigned int flags) {
    char* old_con = nullptr;
    if (lgetfilecon(path, &old_con) < 0) {
        old_con = nullptr;
    }

    int const rc = selinux_restorecon(path, flags);

    if ((flags & SELINUX_RESTORECON_NOCHANGE) == 0 && rc == 0 &&
        (flags & SELINUX_RESTORECON_VERBOSE) != 0) {
        char* new_con = nullptr;
        if (lgetfilecon(path, &new_con) < 0) {
            new_con = nullptr;
        }
        if (old_con != nullptr && new_con != nullptr && strcmp(old_con, new_con) != 0) {
            printf("relabeled %s from %s to %s\n", path, old_con, new_con);
        } else if (old_con != nullptr && new_con == nullptr) {
            printf("relabeled %s from %s (no context)\n", path, old_con);
        } else if (old_con == nullptr && new_con != nullptr) {
            printf("relabeled %s to %s\n", path, new_con);
        }
        freecon(new_con);
    }

    freecon(old_con);
    return rc;
}

static int recursive_callback(const char* fpath, const struct stat* sb,
                               int typeflag, struct FTW* ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    int const rc = restorecon_one_file(fpath, restorecon_glob.flags);
    if (rc != 0) {
        if (restorecon_glob.ignore == 0) {
            (void)fprintf(stderr, "restorecon: %s: failed to relabel\n", fpath);
        }
        restorecon_glob.errors++;
    }
    return 0;
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
        printf("Usage: %s [OPTION]... FILE...\n", argv[0]);
        printf("Restore the default SELinux security context of each FILE.\n");
        printf("\n");
        printf("  -R, --recursive      operate on files and directories recursively\n");
        printf("  -v, --verbose        output a diagnostic for every file processed\n");
        printf("  -n, --nochange       don't change any file labels\n");
        printf("  -F, --force          force reset of customizable and default contexts\n");
        printf("  -i, --ignore-errors  continue on errors\n");
        printf("      --help           display this help and exit\n");
        printf("      --version        output version information and exit\n");
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
    if (recursive_opt->count > 0) { flags |= SELINUX_RESTORECON_RECURSE;
}
    if (verbose_opt->count > 0) { flags |= SELINUX_RESTORECON_VERBOSE;
}
    if (nochange_opt->count > 0) { flags |= SELINUX_RESTORECON_NOCHANGE;
}
    if (force_opt->count > 0) { flags |= SELINUX_RESTORECON_SET_SPECFILE_CTX;
}
    if (ignore_opt->count > 0) { flags |= SELINUX_RESTORECON_COUNT_ERRORS;
}

    restorecon_glob.flags = flags;
    restorecon_glob.errors = 0;
    restorecon_glob.ignore = ignore_opt->count > 0 ? 1 : 0;

    int rc = 0;
    for (int i = 0; i < files_opt->count; i++) {
        const char* path = files_opt->filename[i];
        if (recursive_opt->count > 0) {
            restorecon_glob.errors = 0;
            if (nftw(path, recursive_callback, 64, FTW_PHYS) != 0) {
                (void)fprintf(stderr, "restorecon: %s: traversal failed\n", path);
                rc = 1;
            }
            if (restorecon_glob.errors > 0) { rc = 1;
}
        } else {
            int const frc = restorecon_one_file(path, flags);
            if (frc != 0) {
                if (ignore_opt->count == 0) {
                    (void)fprintf(stderr, "restorecon: %s: failed to relabel\n", path);
                }
                rc = 1;
            }
        }
    }

    return rc;
}

REGISTER_COMMAND("restorecon", restorecon_command, "Restore SELinux security context of files");
