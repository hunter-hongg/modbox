#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/acl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ftw.h>
#include <grp.h>
#include <pwd.h>
#include <acl/libacl.h>
#include "commands/getfacl.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

static const GetfaclOptions *g_opts;

/* ── helpers ───────────────────────────────────────────────────────────── */

static const char *uid_to_string(uid_t uid) {
    static char buf[64];
    const struct passwd *pw = getpwuid(uid);
    if (pw != nullptr) {
        (void)snprintf(buf, sizeof(buf), "%s", pw->pw_name);
    } else {
        (void)snprintf(buf, sizeof(buf), "%u", uid);
    }
    return buf;
}

static const char *gid_to_string(gid_t gid) {
    static char buf[64];
    const struct group *gr = getgrgid(gid);
    if (gr != nullptr) {
        (void)snprintf(buf, sizeof(buf), "%s", gr->gr_name);
    } else {
        (void)snprintf(buf, sizeof(buf), "%u", gid);
    }
    return buf;
}

static const char *mode_to_rwx(mode_t mode) {
    static char buf[4];
    buf[0] = ((mode & 04) != 0u) ? 'r' : '-';
    buf[1] = ((mode & 02) != 0u) ? 'w' : '-';
    buf[2] = ((mode & 01) != 0u) ? 'x' : '-';
    buf[3] = '\0';
    return buf;
}

// Format owner/group strings for the header line.
// When is_numeric is true, raw UID/GID numbers are printed.
static void format_owner_group(const struct stat *st, int is_numeric,
                                char *owner_buf, size_t owner_sz,
                                char *group_buf, size_t group_sz) {
    if (is_numeric != 0) {
        (void)snprintf(owner_buf, owner_sz, "%u", st->st_uid);
        (void)snprintf(group_buf, group_sz, "%u", st->st_gid);
    } else {
        (void)snprintf(owner_buf, owner_sz, "%s", uid_to_string(st->st_uid));
        (void)snprintf(group_buf, group_sz, "%s", gid_to_string(st->st_gid));
    }
}

// Track whether we've warned about stripping leading '/'.
static bool warned_absolute = false;

// Return a display path: strips leading '/' unless -p is set.
// Warns once about stripping.
static const char *display_path(const char *path) {
    if (g_opts->absolute_names != 0) { return path;
}
    if (path[0] == '/') {
        if (!warned_absolute) {
            (void)fprintf(stderr, "getfacl: Removing leading '/' from absolute path names\n");
            warned_absolute = true;
        }
        return path + 1;  // skip leading '/'
    }
    return path;
}

// Print the "# file: / # owner: / # group:" header block.
// Returns 1 if header was printed, 0 if it was omitted.
static int print_header(const char *path, const char *owner, const char *group) {
    if (g_opts->omit_header != 0) { return 0;
}
    printf("# file: %s\n", display_path(path));
    printf("# owner: %s\n", owner);
    printf("# group: %s\n", group);
    return 1;
}

// Build the option flags for acl_to_any_text().
static int acl_text_options() {
    int options = TEXT_SOME_EFFECTIVE;
    if (g_opts->is_numeric != 0) { options |= TEXT_NUMERIC_IDS;
}
    if (g_opts->all_effective != 0) { options = (options & ~TEXT_SOME_EFFECTIVE) | TEXT_ALL_EFFECTIVE;
}
    if (g_opts->no_effective != 0) { options &= ~TEXT_SOME_EFFECTIVE;
}
    if (g_opts->is_tabular != 0) { options |= TEXT_ABBREVIATE;
}
    return options;
}

/* ── print one file's ACL ──────────────────────────────────────────────── */

static int print_file_acl(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        (void)fprintf(stderr, "getfacl: cannot stat '%s': %s\n", path, strerror(errno));
        return 1;
    }

    // Determine what to show
    // GNU behavior: default shows both access and default ACLs
    // -a shows access only, -d shows default only, both show both
    int show_access = 0;
    int show_default = 0;
    if ((g_opts->is_default == 0) && (g_opts->is_access == 0)) {
        show_access = 1;
        show_default = 1;  // GNU default: show both
    } else {
        if (g_opts->is_access != 0) { show_access = 1;
}
        if (g_opts->is_default != 0) { show_default = 1;
}
    }

    int printed_header = 0;

    // -s / --skip-base: skip files that only have base ACL entries
    if (g_opts->skip_base != 0) {
        if ((acl_extended_file(path) == 0) &&
            acl_get_file(path, ACL_TYPE_DEFAULT) == nullptr) {
            return 0;
        }
    }

    char owner_buf[64];
    char group_buf[64];
    format_owner_group(&st, g_opts->is_numeric,
                       owner_buf, sizeof(owner_buf),
                       group_buf, sizeof(group_buf));

    if (show_access != 0) {
        acl_t acl = acl_get_file(path, ACL_TYPE_ACCESS);
        if (acl != nullptr) {
            if (print_header(path, owner_buf, group_buf) != 0) {
                printed_header = 1;
}

            char *text = acl_to_any_text(acl, nullptr, '\n', acl_text_options());
            if (text != nullptr) {
                printf("%s\n", text);
                acl_free(text);
            }
            acl_free(acl);
        } else {
            // No extended ACL — fall back to mode bits
            if (print_header(path, owner_buf, group_buf) != 0) {
                printed_header = 1;
}
            printf("user::%s\n", mode_to_rwx((st.st_mode & S_IRWXU) >> 6));
            printf("group::%s\n", mode_to_rwx((st.st_mode & S_IRWXG) >> 3));
            printf("other::%s\n", mode_to_rwx(st.st_mode & S_IRWXO));
        }
    }

    if (show_default != 0) {
        acl_t def_acl = acl_get_file(path, ACL_TYPE_DEFAULT);
        if (def_acl != nullptr) {
            if (printed_header == 0) {
                printed_header = print_header(path, owner_buf, group_buf);
}

            // With -d only (no -a), show default ACL without "default:" prefix
            const char *prefix = (show_access != 0) ? "default:" : nullptr;
            char *text = acl_to_any_text(def_acl, prefix, '\n', acl_text_options());
            if (text != nullptr) {
                printf("%s\n", text);
                acl_free(text);
            }
            acl_free(def_acl);
        }
    }

    return 0;
}

/* ── recursive nftw callback ───────────────────────────────────────────── */

static int recursive_callback(const char *fpath, const struct stat *sb,
                               int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (typeflag == FTW_DNR || typeflag == FTW_NS) {
        (void)fprintf(stderr, "getfacl: cannot access '%s': %s\n", fpath, strerror(errno));
        return 0;
    }

    print_file_acl(fpath);
    return 0;
}

/* ── command entry point ───────────────────────────────────────────────── */

int getfacl_command(int argc, char **argv) {
    struct arg_lit *recursive_opt =
        arg_lit0("R", "recursive", "process directories recursively");
    struct arg_lit *dereference_opt =
        arg_litn("H", "dereference", 0, 1,
                 "dereference command-line symbolic links");
    struct arg_lit *logical_opt =
        arg_litn("L", "logical", 0, 1,
                 "follow all symbolic links");
    struct arg_lit *physical_opt =
        arg_litn("P", "physical", 0, 1,
                 "do not follow symbolic links (default)");
    struct arg_lit *tabular_opt =
        arg_lit0("t", "tabular", "use tab-separated format (short)");
    struct arg_lit *default_opt =
        arg_lit0("d", "default", "only display default ACL entries");
    struct arg_lit *access_opt =
        arg_lit0("a", "access", "only display access ACL entries (default)");
    struct arg_lit *header_opt =
        arg_lit0("c", "omit-header", "omit leading filename and device lines");
    struct arg_lit *numeric_opt =
        arg_lit0("n", "numeric", "numerical values for UIDs/GIDs");
    struct arg_lit *effective_opt =
        arg_lit0("e", "all-effective", "display effective ACL mask for all entries");
    struct arg_lit *no_effective_opt =
        arg_lit0("E", "no-effective", "don't display effective ACL mask");
    struct arg_lit *skip_base_opt =
        arg_lit0("s", "skip-base", "skip files that only have the base entries");
    struct arg_lit *absolute_names_opt =
        arg_lit0("p", "absolute-names", "do not strip leading '/' in pathnames");
    struct arg_lit *one_fs_opt =
        arg_lit0(nullptr, "one-file-system", "stay within one filesystem");
    struct arg_lit *version_opt =
        arg_lit0("v", "version", "output version info");
    struct arg_lit *help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_lit *preserve_root_opt =
        arg_lit0(nullptr, "preserve-root",
                 "fail to operate recursively on '/'");
    struct arg_lit *no_preserve_root_opt =
        arg_lit0(nullptr, "no-preserve-root",
                 "do not treat '/' specially (the default)");
    struct arg_file *files =
        arg_filen(nullptr, nullptr, "FILE...", 0, 1000, "files to examine");
    struct arg_end *end = arg_end(20);

    ArgTable at({recursive_opt, dereference_opt, logical_opt, physical_opt,
                 tabular_opt, default_opt, access_opt,
                 header_opt, numeric_opt, effective_opt, no_effective_opt,
                 skip_base_opt, absolute_names_opt,
                 one_fs_opt, version_opt, help_opt, preserve_root_opt,
                 no_preserve_root_opt, files, end});

    int const nerrors = at.parse(argc, argv);

    // Version check first
    if (version_opt->count > 0) {
        printf("modbox getfacl (modbox)\n");
        printf("Copyright (C) Free Software Foundation. License GPLv3+\n");
        printf("This is free software: you are free to change and redistribute it.\n");
        printf("There is NO WARRANTY, to the extent permitted by law.\n");
        return 0;
    }

    if (help_opt->count > 0) {
        printf("Usage: getfacl [-aceEsRLPtpndvh] file ...\n");
        printf("Display Access Control Lists\n");
        printf("\n");
        printf("  -a, --access             display the file access ACL only\n");
        printf("  -c, --omit-header        do not display the comment header\n");
        printf("  -d, --default            display the default ACL only\n");
        printf("  -e, --all-effective      print all effective rights comments\n");
        printf("  -E, --no-effective       print no effective rights comments\n");
        printf("  -s, --skip-base          skip files that only have the base entries\n");
        printf("  -R, --recursive          recurse into subdirectories\n");
        printf("  -L, --logical            logical walk, follow symbolic links\n");
        printf("  -P, --physical           physical walk, do not follow symbolic links\n");
        printf("  -p, --absolute-names     do not strip leading '/' in pathnames\n");
        printf("  -t, --tabular            use tabular output format\n");
        printf("  -n, --numeric            print numeric user/group identifiers\n");
        printf("      --one-file-system    skip files on different filesystems\n");
        printf("      --preserve-root      fail to operate recursively on '/'\n");
        printf("      --no-preserve-root   do not treat '/' specially (the default)\n");
        printf("  -v, --version            output version information\n");
        printf("  -h, --help               display this help and exit\n");
        printf("\n");
        printf("Default behavior displays access ACL with headers.\n");
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // Populate options struct
    GetfaclOptions opts{};
    opts.is_recursive = static_cast<int>(recursive_opt->count > 0);
    opts.is_dereference = static_cast<int>(dereference_opt->count > 0);
    opts.is_logical = static_cast<int>(logical_opt->count > 0);
    opts.is_physical = static_cast<int>(physical_opt->count > 0);
    opts.is_tabular = static_cast<int>(tabular_opt->count > 0);
    opts.is_default = static_cast<int>(default_opt->count > 0);
    opts.is_access = static_cast<int>(access_opt->count > 0);
    opts.omit_header = static_cast<int>(header_opt->count > 0);
    opts.is_numeric = static_cast<int>(numeric_opt->count > 0);
    opts.all_effective = static_cast<int>(effective_opt->count > 0);
    opts.no_effective = static_cast<int>(no_effective_opt->count > 0);
    opts.skip_base = static_cast<int>(skip_base_opt->count > 0);
    opts.absolute_names = static_cast<int>(absolute_names_opt->count > 0);
    opts.one_file_system = static_cast<int>(one_fs_opt->count > 0);
    opts.preserve_root = static_cast<int>(preserve_root_opt->count > 0);
    // Build nftw flags
    int nftw_flags = FTW_PHYS; // default physical
    if (opts.is_logical != 0) {
        nftw_flags &= ~FTW_PHYS;
    }
    if (opts.is_recursive != 0) {
        nftw_flags |= FTW_DEPTH;
    }
    // one-file-system: skip mount points
    if (opts.one_file_system != 0) {
        // FTW_MOUNT is not always available; skip on systems without it
#ifdef FTW_MOUNT
        nftw_flags |= FTW_MOUNT;
#endif
    }

    g_opts = &opts;

    // Process files
    if (opts.is_recursive == 0) {
        int rc = 0;
        int const n = files->count;
        for (int i = 0; i < n; i++) {
            if (print_file_acl(files->filename[i]) != 0) {
                rc = 1;
}
            if (i < n - 1) { printf("\n");
}
        }
        return rc;
    } else {
        int const n = files->count;
        for (int i = 0; i < n; i++) {
            const char *path = files->filename[i];
            char resolved[4096];

            // -H: dereference command-line symlinks before starting traversal
            if (opts.is_dereference != 0) {
                struct stat lst;
                if (lstat(path, &lst) == 0 && S_ISLNK(lst.st_mode)) {
                    if (realpath(path, resolved) != nullptr) {
                        path = resolved;
                    }
                }
            }

            // preserve-root: check before starting traversal
            if ((opts.preserve_root != 0) && strcmp(path, "/") == 0) {
                (void)fprintf(stderr, "getfacl: it is dangerous to operate recursively on '/'\n");
                (void)fprintf(stderr, "getfacl: use --no-preserve-root to override this failsafe\n");
                return 1;
            }
            if (nftw(path, recursive_callback, 20, nftw_flags) != 0) {
                (void)fprintf(stderr, "getfacl: failed to traverse '%s': %s\n",
                        path, strerror(errno));
                return 1;
            }
            if (i < n - 1) { printf("\n");
}
        }
    }

    return 0;
}

REGISTER_COMMAND("getfacl", getfacl_command, "Display access control lists")
