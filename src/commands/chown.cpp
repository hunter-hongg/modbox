#include <argtable3.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ftw.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "commands/chown.hpp"
#include "commands/command_macros.hpp"

/* ── File-scope globals for nftw callback ──────────────────────────────── */

static const ChownOptions *chown_glob_opts;
static int chown_errors;
static int chown_changes_made;

/* ── Name resolution ───────────────────────────────────────────────────── */

static uid_t resolve_uid(const char *name) {
    char *end;
    long val = strtol(name, &end, 10);
    if (*end == '\0' && val >= 0 && val <= (long)UINT32_MAX) {
        return (uid_t)val;
    }

    struct passwd *pw = getpwnam(name);
    if (!pw) {
        return (uid_t)-1;
    }
    return pw->pw_uid;
}

static gid_t resolve_gid(const char *name) {
    char *end;
    long val = strtol(name, &end, 10);
    if (*end == '\0' && val >= 0 && val <= (long)UINT32_MAX) {
        return (gid_t)val;
    }

    struct group *gr = getgrnam(name);
    if (!gr) {
        return (gid_t)-1;
    }
    return gr->gr_gid;
}

/* Parse [OWNER][:[GROUP]] — both sides optional but at least one must be set */
static int parse_owner_group(const char *spec, uid_t *owner, gid_t *group,
                             int *owner_set, int *group_set) {
    const char *colon = strchr(spec, ':');

    *owner_set = 0;
    *group_set = 0;

    if (!colon) {
        /* just OWNER */
        uid_t u = resolve_uid(spec);
        if (u == (uid_t)-1) {
            return -1;
        }
        *owner = u;
        *owner_set = 1;
        return 0;
    }

    /* Has a colon */
    if (colon > spec) {
        /* OWNER:… */
        size_t len = (size_t)(colon - spec);
        char *owner_str = (char*)malloc(len + 1);
        if (!owner_str) {
            return -1;
        }
        memcpy(owner_str, spec, len);
        owner_str[len] = '\0';
        uid_t u = resolve_uid(owner_str);
        free(owner_str);
        if (u == (uid_t)-1) {
            return -1;
        }
        *owner = u;
        *owner_set = 1;
    }

    if (colon[1] != '\0') {
        /* …:GROUP */
        gid_t g = resolve_gid(colon + 1);
        if (g == (gid_t)-1) {
            return -1;
        }
        *group = g;
        *group_set = 1;
    } else if (*owner_set) {
        /* OWNER: — set group to owner's login group */
        struct passwd *pw = getpwuid(*owner);
        if (pw) {
            *group = pw->pw_gid;
            *group_set = 1;
        }
    }

    return 0;
}

/* ── Single-file chown ─────────────────────────────────────────────────── */

static int chown_one_file(const char *path, const ChownOptions *opts) {
    if (opts->preserve_root && strcmp(path, "/") == 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chown: it is dangerous to operate recursively on '/'\n");
        }
        return 1;
    }

    /* --from filter */
    if (opts->has_from) {
        struct stat st;
        int stat_rc = (opts->no_dereference)
                          ? lstat(path, &st)
                          : stat(path, &st);
        if (stat_rc != 0) {
            if (!opts->is_silent) {
                fprintf(stderr, "chown: cannot access '%s': %s\n", path, strerror(errno));
            }
            return 1;
        }
        if (opts->from_owner != (uid_t)-1 && st.st_uid != opts->from_owner) {
            return 1;
        }
        if (opts->from_group != (gid_t)-1 && st.st_gid != opts->from_group) {
            return 1;
        }
    }

    struct stat st_before;
    int have_before = (opts->is_verbose || opts->is_changes)
                          ? (lstat(path, &st_before) == 0)
                          : 0;

    int rc;
    if (opts->no_dereference) {
        rc = lchown(path, opts->owner, opts->group);
    } else {
        rc = chown(path, opts->owner, opts->group);
    }

    if (rc != 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chown: changing ownership of '%s': %s\n", path, strerror(errno));
        }
        return 1;
    }

    chown_changes_made = 1;

    if (opts->is_verbose || opts->is_changes) {
        int changed = 1;
        if (opts->is_changes && have_before) {
            struct stat st_after;
            if (lstat(path, &st_after) == 0) {
                changed = (st_before.st_uid != st_after.st_uid ||
                           st_before.st_gid != st_after.st_gid);
            }
        }
        if (changed) {
            struct stat st;
            if (lstat(path, &st) == 0) {
                struct passwd *pw = getpwuid(st.st_uid);
                struct group *gr = getgrgid(st.st_gid);
                printf("changed ownership of '%s' from %s:%s to %s:%s\n",
                       path,
                       pw ? pw->pw_name : "?",
                       gr ? gr->gr_name : "?",
                       pw ? pw->pw_name : "?",
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

    if (chown_glob_opts->preserve_root && strcmp(fpath, "/") == 0) {
        if (!chown_glob_opts->is_silent) {
            fprintf(stderr, "chown: it is dangerous to operate recursively on '/'\n");
        }
        chown_errors = 1;
        return 0;
    }

    return chown_one_file(fpath, chown_glob_opts);
}

/* ── Command entry point ───────────────────────────────────────────────── */

void chown_command(int argc, char **argv) {
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
    struct arg_str *from_opt =
        arg_str0(NULL, "from", "CURRENT_OWNER:CURRENT_GROUP",
                 "change the owner/group of each file only if its current owner/group matches");
    struct arg_lit *preserve_root_opt =
        arg_lit0(NULL, "preserve-root", "fail to operate recursively on '/'");
    struct arg_lit *no_preserve_root_opt =
        arg_lit0(NULL, "no-preserve-root", "do not treat '/' specially (the default)");
    struct arg_str *reference_opt =
        arg_str0(NULL, "reference", "RFILE", "use RFILE's owner and group instead of specifying values");
    struct arg_lit *traverse_H_opt =
        arg_lit0("H", NULL, "if -R, follow symlinks on the command line");
    struct arg_lit *traverse_L_opt =
        arg_lit0("L", NULL, "if -R, follow all symlinks");
    struct arg_lit *traverse_P_opt =
        arg_lit0("P", NULL, "if -R, do not follow any symlinks (the default)");
    struct arg_lit *help_opt =
        arg_lit0(NULL, "help", "display this help and exit");
    struct arg_file *all_args =
        arg_filen(NULL, NULL, "OWNER[:[GROUP]] FILE...", 0, 1000, "owner and files to change");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        recursive_opt, verbose_opt, changes_opt, silent_opt, quiet_opt,
        dereference_opt, no_dereference_opt, from_opt,
        preserve_root_opt, no_preserve_root_opt, reference_opt,
        traverse_H_opt, traverse_L_opt, traverse_P_opt,
        help_opt, all_args, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [OWNER][:[GROUP]] FILE...\n", argv[0]);
        printf("  or:  %s [OPTION]... --reference=RFILE FILE...\n", argv[0]);
        printf("Change the owner and/or group of each FILE to OWNER and/or GROUP.\n");
        printf("With --reference, change the owner and group of each FILE to those of RFILE.\n");
        printf("\n");
        printf("  -c, --changes          like verbose but report only when a change is made\n");
        printf("  -f, --silent, --quiet  suppress most error messages\n");
        printf("  -v, --verbose          output a diagnostic for every file processed\n");
        printf("      --dereference      affect the referent of each symbolic link (the default)\n");
        printf("  -h, --no-dereference   affect symbolic links instead of any referent\n");
        printf("      --from=CURRENT_OWNER:CURRENT_GROUP\n");
        printf("                         change the owner/group of each file only if its\n");
        printf("                         current owner/group matches those specified here\n");
        printf("      --no-preserve-root do not treat '/' specially (the default)\n");
        printf("      --preserve-root    fail to operate recursively on '/'\n");
        printf("      --reference=RFILE  use RFILE's owner and group instead of specifying\n");
        printf("                         OWNER:GROUP values\n");
        printf("  -R, --recursive        operate on files and directories recursively\n");
        printf("  -H                     if -R, follow symlinks on the command line\n");
        printf("  -L                     if -R, follow all symlinks\n");
        printf("  -P                     if -R, do not follow any symlinks (the default)\n");
        printf("      --help            display this help and exit\n");
        printf("\n");
        printf("If OWNER is omitted, the group is changed.  If GROUP is omitted, the group\n");
        printf("of each file is not changed.  If a colon but no group follows, the group is\n");
        printf("changed to OWNER's login group.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    ChownOptions opts = {};
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

    /* --from parsing */
    if (from_opt->count > 0) {
        const char *from_spec = from_opt->sval[0];
        uid_t fu = (uid_t)-1;
        gid_t fg = (gid_t)-1;
        int fu_set = 0, fg_set = 0;
        if (parse_owner_group(from_spec, &fu, &fg, &fu_set, &fg_set) != 0) {
            fprintf(stderr, "%s: invalid --from value: '%s'\n", argv[0], from_spec);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        opts.has_from = 1;
        if (fu_set) opts.from_owner = fu;
        if (fg_set) opts.from_group = fg;
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
        /* First positional arg is the owner/group spec */
        const char *spec = all_args->filename[0];
        file_offset = 1;
        num_files--;

        if (parse_owner_group(spec, &opts.owner, &opts.group,
                               &opts.owner_set, &opts.group_set) != 0) {
            fprintf(stderr, "%s: invalid owner: '%s'\n", argv[0], spec);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
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
        opts.owner = ref_st.st_uid;
        opts.group = ref_st.st_gid;
        opts.owner_set = 1;
        opts.group_set = 1;
    }

    chown_errors = 0;
    chown_changes_made = 0;

    /* Determine nftw flags */
    int nftw_opts = FTW_PHYS;        /* default: -P: don't follow any symlinks */
    if (opts.traverse_mode == 2) {   /* -L: follow all symlinks */
        nftw_opts = 0;
    }
    /* -H: follow cmdline symlinks — nftw always descends through cmdline
       arguments, so FTW_PHYS suffices (it won't follow during traversal) */

    chown_glob_opts = &opts;

    if (opts.is_recursive) {
        for (int i = 0; i < num_files; i++) {
            const char *path = all_args->filename[file_offset + i];
            if (nftw(path, recursive_callback, 20, nftw_opts) != 0) {
                if (!opts.is_silent) {
                    fprintf(stderr, "chown: %s: %s\n", path, strerror(errno));
                }
                chown_errors = 1;
            }
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            const char *path = all_args->filename[file_offset + i];
            if (chown_one_file(path, &opts) != 0) {
                chown_errors = 1;
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("chown", chown_command, "Change file owner and group");
