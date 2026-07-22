#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ftw.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "commands/chgrp.hpp"
#include "commands/command_macros.hpp"

/* ── File-scope globals for nftw callback ──────────────────────────────── */

static const ChgrpOptions *chgrp_glob_opts;
static int chgrp_errors;
static int chgrp_changes_made;

/* ── Name resolution ───────────────────────────────────────────────────── */

static gid_t resolve_gid(const char *name) {
    char *end;
    long val = strtol(name, &end, 10);
    if (*end == '\0' && val >= 0) {
        return (gid_t)val;
    }

    struct group *gr = getgrnam(name);
    if (!gr) {
        return (gid_t)-1;
    }
    return gr->gr_gid;
}

/* ── Single-file chgrp ─────────────────────────────────────────────────── */

static int chgrp_one_file(const char *path, const ChgrpOptions *opts) {
    if (opts->preserve_root && strcmp(path, "/") == 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chgrp: it is dangerous to operate recursively on '/'\n");
        }
        return 1;
    }

    struct stat st_before;
    int have_before = (opts->is_verbose || opts->is_changes)
                          ? (lstat(path, &st_before) == 0)
                          : 0;

    int rc;
    if (opts->no_dereference) {
        rc = lchown(path, (uid_t)-1, opts->group);
    } else {
        rc = chown(path, (uid_t)-1, opts->group);
    }

    if (rc != 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chgrp: changing group of '%s': %s\n", path, strerror(errno));
        }
        return 1;
    }

    chgrp_changes_made = 1;

    if (opts->is_verbose || opts->is_changes) {
        int changed = 1;
        if (opts->is_changes && have_before) {
            struct stat st_after;
            if (lstat(path, &st_after) == 0) {
                changed = (st_before.st_gid != st_after.st_gid);
            }
        }
        if (changed) {
            struct stat st;
            if (lstat(path, &st) == 0) {
                struct group *gr = getgrgid(st.st_gid);
                printf("changed group of '%s' from %s to %s\n",
                       path,
                       gr ? gr->gr_name : "?",
                       gr ? gr->gr_name : "?");
            }
        }
    }

    return 0;
}

/* ── Recursive nftw callback ───────────────────────────────────────────── */

static int recursive_callback(const char *fpath, const struct stat *sb,
                               int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;

    if (chgrp_glob_opts->preserve_root && strcmp(fpath, "/") == 0) {
        if (!chgrp_glob_opts->is_silent) {
            fprintf(stderr, "chgrp: it is dangerous to operate recursively on '/'\n");
        }
        chgrp_errors = 1;
        return 0;
    }

    return chgrp_one_file(fpath, chgrp_glob_opts);
}

/* ── Command entry point ───────────────────────────────────────────────── */

void chgrp_command(int argc, char **argv) {
    struct arg_lit *recursive_opt =
        arg_lit0("R", "recursive", "operate on files and directories recursively");
    struct arg_lit *verbose_opt =
        arg_lit0("v", "verbose", "output a diagnostic for every file processed");
    struct arg_lit *changes_opt =
        arg_lit0("c", "changes", "like verbose but report only when a change is made");
    struct arg_lit *silent_opt =
        arg_litn("f", "silent", 0, 1, "suppress most error messages");
    struct arg_lit *quiet_opt =
        arg_lit0(NULL, "quiet", "suppress most error messages");
    struct arg_lit *dereference_opt =
        arg_lit0(NULL, "dereference", "affect the referent of each symbolic link (the default)");
    struct arg_lit *no_dereference_opt =
        arg_lit0("h", "no-dereference", "affect symbolic links instead of any referent");
    struct arg_lit *preserve_root_opt =
        arg_lit0(NULL, "preserve-root", "fail to operate recursively on '/'");
    struct arg_lit *no_preserve_root_opt =
        arg_lit0(NULL, "no-preserve-root", "do not treat '/' specially (the default)");
    struct arg_str *reference_opt =
        arg_str0(NULL, "reference", "RFILE", "use RFILE's group instead of specifying a value");
    struct arg_lit *traverse_H_opt =
        arg_lit0("H", NULL, "if -R, follow symlinks on the command line");
    struct arg_lit *traverse_L_opt =
        arg_lit0("L", NULL, "if -R, follow all symlinks");
    struct arg_lit *traverse_P_opt =
        arg_lit0("P", NULL, "if -R, do not follow any symlinks (the default)");
    struct arg_lit *help_opt =
        arg_lit0(NULL, "help", "display this help and exit");
    struct arg_file *all_args =
        arg_filen(NULL, NULL, "GROUP FILE...", 0, 1000, "group and files to change");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        recursive_opt, verbose_opt, changes_opt, silent_opt, quiet_opt,
        dereference_opt, no_dereference_opt,
        preserve_root_opt, no_preserve_root_opt, reference_opt,
        traverse_H_opt, traverse_L_opt, traverse_P_opt,
        help_opt, all_args, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... GROUP FILE...\n", argv[0]);
        printf("  or:  %s [OPTION]... --reference=RFILE FILE...\n", argv[0]);
        printf("Change the group of each FILE to GROUP.\n");
        printf("With --reference, change the group of each FILE to that of RFILE.\n");
        printf("\n");
        printf("  -c, --changes          like verbose but report only when a change is made\n");
        printf("  -f, --silent, --quiet  suppress most error messages\n");
        printf("  -v, --verbose          output a diagnostic for every file processed\n");
        printf("      --dereference      affect the referent of each symbolic link (the default)\n");
        printf("  -h, --no-dereference   affect symbolic links instead of any referent\n");
        printf("      --no-preserve-root do not treat '/' specially (the default)\n");
        printf("      --preserve-root    fail to operate recursively on '/'\n");
        printf("      --reference=RFILE  use RFILE's group instead of specifying a value\n");
        printf("  -R, --recursive        operate on files and directories recursively\n");
        printf("  -H                     if -R, follow symlinks on the command line\n");
        printf("  -L                     if -R, follow all symlinks\n");
        printf("  -P                     if -R, do not follow any symlinks (the default)\n");
        printf("      --help            display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    ChgrpOptions opts = {};
    opts.is_recursive = (recursive_opt->count > 0);
    opts.is_verbose = (verbose_opt->count > 0);
    opts.is_changes = (changes_opt->count > 0);
    opts.is_silent = (silent_opt->count > 0 || quiet_opt->count > 0);
    opts.no_dereference = (no_dereference_opt->count > 0);
    opts.preserve_root = (preserve_root_opt->count > 0);
    opts.reference = (reference_opt->count > 0) ? reference_opt->sval[0] : nullptr;

    if (traverse_L_opt->count > 0) {
        opts.traverse_mode = 2;
    } else if (traverse_H_opt->count > 0) {
        opts.traverse_mode = 1;
    } else {
        opts.traverse_mode = 0;
    }

    int num_files = all_args->count;
    int file_offset = 0;

    if (reference_opt->count == 0) {
        if (num_files < 1) {
            fprintf(stderr, "%s: missing operand\n", argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        const char *spec = all_args->filename[0];
        file_offset = 1;
        num_files--;

        if (num_files == 0) {
            fprintf(stderr, "%s: missing operand\n", argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        gid_t g = resolve_gid(spec);
        if (g == (gid_t)-1) {
            fprintf(stderr, "%s: invalid group: '%s'\n", argv[0], spec);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        opts.group = g;
        opts.group_set = 1;
    } else {
        if (num_files == 0) {
            fprintf(stderr, "%s: missing operand\n", argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        /* --reference: stat the reference file */
        struct stat ref_st;
        if (stat(reference_opt->sval[0], &ref_st) != 0) {
            fprintf(stderr, "%s: cannot access '%s': %s\n", argv[0],
                    reference_opt->sval[0], strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        opts.group = ref_st.st_gid;
        opts.group_set = 1;
    }

    chgrp_errors = 0;
    chgrp_changes_made = 0;

    int nftw_opts = FTW_PHYS;
    if (opts.traverse_mode == 2) {
        nftw_opts = 0;
    }

    chgrp_glob_opts = &opts;

    if (opts.is_recursive) {
        for (int i = 0; i < num_files; i++) {
            const char *path = all_args->filename[file_offset + i];
            if (nftw(path, recursive_callback, 20, nftw_opts) != 0) {
                if (!opts.is_silent) {
                    fprintf(stderr, "chgrp: %s: %s\n", path, strerror(errno));
                }
                chgrp_errors = 1;
            }
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            const char *path = all_args->filename[file_offset + i];
            if (chgrp_one_file(path, &opts) != 0) {
                chgrp_errors = 1;
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("chgrp", chgrp_command, "Change group ownership");
