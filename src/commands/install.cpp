#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "commands/install.hpp"

#define COPY_BUF_SIZE 8192
#define MAX_FILES 4096
#define PATH_BUF_SIZE 4096

static int parse_mode(const char *mode_str, mode_t *out) {
    char *end;
    long val = strtol(mode_str, &end, 8);
    if (*end == '\0' && val >= 0 && val <= 07777) {
        *out = (mode_t)(val & 07777);
        return 0;
    }
    return -1;
}

static int files_match(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb");
    if (!fa) return 0;
    FILE *fb = fopen(b, "rb");
    if (!fb) {
        fclose(fa);
        return 0;
    }

    char buf_a[COPY_BUF_SIZE];
    char buf_b[COPY_BUF_SIZE];
    int match = 1;

    while (1) {
        size_t na = fread(buf_a, 1, sizeof(buf_a), fa);
        size_t nb = fread(buf_b, 1, sizeof(buf_b), fb);
        if (na != nb || (na > 0 && memcmp(buf_a, buf_b, na) != 0)) {
            match = 0;
            break;
        }
        if (na == 0) break;
    }

    fclose(fa);
    fclose(fb);
    return match;
}

static int make_backup(const char *path, const char *suffix) {
    char backup[PATH_BUF_SIZE];
    const char *sfx = suffix ? suffix : "~";
    int n = snprintf(backup, sizeof(backup), "%s%s", path, sfx);
    if (n < 0 || (size_t)n >= sizeof(backup)) {
        fprintf(stderr, "install: backup path too long\n");
        return -1;
    }
    if (rename(path, backup) != 0) {
        fprintf(stderr, "install: cannot create backup '%s': %s\n",
                backup, strerror(errno));
        return -1;
    }
    return 0;
}

static uid_t resolve_uid(const char *name) {
    char *end;
    long val = strtol(name, &end, 10);
    if (*end == '\0' && val >= 0) return (uid_t)val;

    struct passwd *pw = getpwnam(name);
    if (!pw) return (uid_t)-1;
    return pw->pw_uid;
}

static gid_t resolve_gid(const char *name) {
    char *end;
    long val = strtol(name, &end, 10);
    if (*end == '\0' && val >= 0) return (gid_t)val;

    struct group *gr = getgrnam(name);
    if (!gr) return (gid_t)-1;
    return gr->gr_gid;
}

static int install_file(const char *src, const char *dst,
                        const InstallOptions *opts) {
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
        fprintf(stderr, "install: cannot stat '%s': %s\n",
                src, strerror(errno));
        return -1;
    }

    if (!S_ISREG(src_stat.st_mode)) {
        fprintf(stderr, "install: '%s' is not a regular file\n", src);
        return -1;
    }

    struct stat dst_stat_buf_lcl;
    int dst_exists = (stat(dst, &dst_stat_buf_lcl) == 0);

    /* -C: skip copy if files match */
    if (opts->is_compare && dst_exists) {
        struct stat dst_stat_buf;
        if (stat(dst, &dst_stat_buf) == 0 && files_match(src, dst)) {
            if (opts->is_verbose) {
                printf("'%s' -> '%s' (skipped: identical)\n", src, dst);
            }
            return 0;
        }
    }

    /* -b: backup existing destination */
    if (opts->is_backup && dst_exists) {
        if (make_backup(dst, opts->backup_suffix) != 0) return -1;
    }

    if (opts->is_verbose) {
        printf("'%s' -> '%s'\n", src, dst);
    }

    FILE *fsrc = fopen(src, "rb");
    if (!fsrc) {
        fprintf(stderr, "install: cannot open '%s': %s\n",
                src, strerror(errno));
        return -1;
    }

    FILE *fdst = fopen(dst, "wb");
    if (!fdst) {
        fprintf(stderr, "install: cannot create '%s': %s\n",
                dst, strerror(errno));
        fclose(fsrc);
        return -1;
    }

    char buf[COPY_BUF_SIZE];
    size_t nread;
    while ((nread = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        if (fwrite(buf, 1, nread, fdst) != nread) {
            fprintf(stderr, "install: error writing to '%s'\n", dst);
            fclose(fsrc);
            fclose(fdst);
            return -1;
        }
    }

    if (ferror(fsrc)) {
        fprintf(stderr, "install: error reading from '%s'\n", src);
        fclose(fsrc);
        fclose(fdst);
        return -1;
    }

    fclose(fsrc);

    /* Set mode before ownership (clears setuid/setgid when chown later) */
    if (opts->mode_set) {
        fchmod(fileno(fdst), opts->mode);
    }

    /* Set ownership */
    if (opts->owner || opts->group) {
        uid_t uid = opts->owner ? resolve_uid(opts->owner) : (uid_t)-1;
        gid_t gid = opts->group ? resolve_gid(opts->group) : (gid_t)-1;

        if (uid == (uid_t)-1 && opts->owner) {
            fprintf(stderr, "install: unknown user '%s'\n", opts->owner);
            fclose(fdst);
            return -1;
        }
        if (gid == (gid_t)-1 && opts->group) {
            fprintf(stderr, "install: unknown group '%s'\n", opts->group);
            fclose(fdst);
            return -1;
        }

        if (fchown(fileno(fdst), uid, gid) != 0) {
            if (errno != EPERM) {
                fprintf(stderr, "install: cannot set ownership of '%s': %s\n",
                        dst, strerror(errno));
            }
        }

        /* chown may clear setuid/setgid; reapply mode */
        if (opts->mode_set) {
            fchmod(fileno(fdst), opts->mode);
        }
    }

    /* Preserve timestamps */
    if (opts->is_preserve_timestamps) {
        fflush(fdst);
        struct timespec times[2];
        times[0] = src_stat.st_atim;
        times[1] = src_stat.st_mtim;
        futimens(fileno(fdst), times);
    }

    fclose(fdst);

    /* Strip symbols */
    if (opts->is_strip) {
        const char *prog = opts->strip_program ? opts->strip_program : "strip";
        char cmd[PATH_BUF_SIZE];
        int n = snprintf(cmd, sizeof(cmd), "%s \"%s\" 2>/dev/null", prog, dst);
        if (n < 0 || (size_t)n >= sizeof(cmd)) {
            fprintf(stderr, "install: strip command too long\n");
            return -1;
        }
        if (system(cmd) != 0) {
            fprintf(stderr, "install: strip failed on '%s'\n", dst);
            return -1;
        }
    }

    return 0;
}

static int create_dir(const char *path, mode_t mode, int is_verbose) {
    if (mkdir(path, mode) != 0) {
        if (errno == EEXIST) {
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                /* Set mode on existing directory */
                if (chmod(path, mode) != 0) {
                    fprintf(stderr, "install: cannot set mode of '%s': %s\n",
                            path, strerror(errno));
                    return -1;
                }
                return 0;
            }
            fprintf(stderr, "install: cannot create directory '%s': %s\n",
                    path, strerror(errno));
            return -1;
        }
        fprintf(stderr, "install: cannot create directory '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    if (chmod(path, mode) != 0) {
        fprintf(stderr, "install: cannot set mode of '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    if (is_verbose) {
        printf("created directory '%s'\n", path);
    }

    return 0;
}

static int create_dir_parents(const char *path, mode_t mode, int is_verbose) {
    char *path_copy = strdup(path);
    if (!path_copy) return -1;

    int ret = 0;
    char *p = path_copy;
    char *sep;

    if (*p == '/') p++;

    while ((sep = strchr(p, '/')) != NULL) {
        *sep = '\0';
        struct stat st;
        if (stat(path_copy, &st) != 0) {
            if (create_dir(path_copy, mode, is_verbose) != 0) {
                ret = -1;
                goto done;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "install: '%s' is not a directory\n", path_copy);
            ret = -1;
            goto done;
        }
        *sep = '/';
        p = sep + 1;
    }
    /* Create final component */
    struct stat final_st;
    if (stat(path_copy, &final_st) != 0) {
        if (create_dir(path_copy, mode, is_verbose) != 0) {
            ret = -1;
        }
    } else if (!S_ISDIR(final_st.st_mode)) {
        fprintf(stderr, "install: '%s' is not a directory\n", path_copy);
        ret = -1;
    }

done:
    free(path_copy);
    return ret;
}

void install_command(int argc, char **argv) {
    struct arg_lit *dir_opt =
        arg_lit0("d", "directory",
                 "create directories instead of copying files");
    struct arg_str *mode_opt =
        arg_str0("m", "mode", "MODE",
                 "set permission mode (as in chmod)");
    struct arg_str *owner_opt =
        arg_str0("o", "owner", "OWNER",
                 "set ownership (super-user only)");
    struct arg_str *group_opt =
        arg_str0("g", "group", "GROUP",
                 "set group ownership");
    struct arg_lit *strip_opt =
        arg_lit0("s", "strip", "strip symbol tables");
    struct arg_str *strip_prog_opt =
        arg_str0(NULL, "strip-program", "PROGRAM",
                 "program used to strip binaries");
    struct arg_lit *compare_opt =
        arg_lit0("C", "compare",
                 "compare each pair of source and destination files, and if "
                 "the destination is identical, do not modify it at all");
    struct arg_lit *preserve_timestamps_opt =
        arg_lit0("p", "preserve-timestamps",
                 "apply source timestamps to destination files");
    struct arg_lit *verbose_opt =
        arg_lit0("v", "verbose", "print the name of each file as it is created");
    struct arg_lit *backup_opt =
        arg_lit0("b", "backup",
                 "make a backup of each existing destination file");
    struct arg_str *suffix_opt =
        arg_str0("S", "suffix", "SUFFIX",
                 "override the usual backup suffix");
    struct arg_str *target_dir_opt =
        arg_str0("t", "target-directory", "DIRECTORY",
                 "copy all SOURCE arguments into DIRECTORY");
    struct arg_lit *no_target_dir_opt =
        arg_lit0("T", "no-target-directory",
                 "treat DEST as a normal file");
    struct arg_lit *help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_file *files_arg =
        arg_filen(NULL, NULL, "SOURCE... DEST|DIRECTORY...", 1, MAX_FILES,
                  "files to install or directories to create");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        dir_opt, mode_opt, owner_opt, group_opt,
        strip_opt, strip_prog_opt, compare_opt,
        preserve_timestamps_opt, verbose_opt,
        backup_opt, suffix_opt, target_dir_opt,
        no_target_dir_opt, help_opt, files_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [-T] SOURCE DEST\n", argv[0]);
        printf("  or:  %s [OPTION]... SOURCE... DIRECTORY\n", argv[0]);
        printf("  or:  %s [OPTION]... -t DIRECTORY SOURCE...\n", argv[0]);
        printf("  or:  %s [OPTION]... -d DIRECTORY...\n", argv[0]);
        printf("\n");
        printf("In the first three forms, copy SOURCE to DEST or multiple SOURCE(s) to\n");
        printf("the existing DIRECTORY, while setting permission modes and owner/group.\n");
        printf("In the fourth form, create all components of the specified DIRECTORY(ies).\n");
        printf("\n");
        printf("  -d, --directory       create directories instead of copying files\n");
        printf("  -m, --mode=MODE       set permission mode (as in chmod)\n");
        printf("  -o, --owner=OWNER     set ownership (super-user only)\n");
        printf("  -g, --group=GROUP     set group ownership\n");
        printf("  -s, --strip           strip symbol tables\n");
        printf("      --strip-program=PROGRAM  program used to strip binaries\n");
        printf("  -C, --compare         compare before copying (don't modify identical files)\n");
        printf("  -p, --preserve-timestamps   preserve source timestamps\n");
        printf("  -v, --verbose         print name of each file as it is created\n");
        printf("  -b, --backup          make a backup of each existing destination file\n");
        printf("  -S, --suffix=SUFFIX   override the usual backup suffix\n");
        printf("  -t, --target-directory=DIRECTORY  copy all SOURCE arguments into DIRECTORY\n");
        printf("  -T, --no-target-directory  treat DEST as a normal file\n");
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

    InstallOptions opts = {};

    opts.is_directory = (dir_opt->count > 0);
    opts.is_verbose = (verbose_opt->count > 0);
    opts.is_strip = (strip_opt->count > 0);
    opts.is_compare = (compare_opt->count > 0);
    opts.is_preserve_timestamps = (preserve_timestamps_opt->count > 0);
    opts.is_backup = (backup_opt->count > 0);
    opts.no_target_directory = (no_target_dir_opt->count > 0);
    opts.owner = (owner_opt->count > 0) ? owner_opt->sval[0] : nullptr;
    opts.group = (group_opt->count > 0) ? group_opt->sval[0] : nullptr;
    opts.target_dir = (target_dir_opt->count > 0) ? target_dir_opt->sval[0] : nullptr;
    opts.strip_program = (strip_prog_opt->count > 0) ? strip_prog_opt->sval[0] : nullptr;
    opts.backup_suffix = (suffix_opt->count > 0) ? suffix_opt->sval[0] : nullptr;

    /* Parse mode */
    if (mode_opt->count > 0) {
        if (parse_mode(mode_opt->sval[0], &opts.mode) != 0) {
            fprintf(stderr, "install: invalid mode '%s'\n", mode_opt->sval[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        opts.mode_set = 1;
    }

    /* -d mode: create directories */
    if (opts.is_directory) {
        if (files_arg->count < 1) {
            fprintf(stderr, "install: missing directory operand\n");
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        for (int i = 0; i < files_arg->count; i++) {
            if (create_dir_parents(files_arg->filename[i], opts.mode,
                                   opts.is_verbose) != 0) {
                /* error already printed */
            }
        }
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* File copy mode */
    int num_files = files_arg->count;
    const char *dst = nullptr;

    if (opts.target_dir != nullptr) {
        /* -t: all positional args are sources, dest from option */
        dst = opts.target_dir;

        struct stat tgt_stat;
        if (stat(dst, &tgt_stat) != 0) {
            fprintf(stderr, "install: target directory '%s' does not exist\n", dst);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        if (!S_ISDIR(tgt_stat.st_mode)) {
            fprintf(stderr, "install: target '%s' is not a directory\n", dst);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        if (num_files < 1) {
            fprintf(stderr, "install: missing file operand\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    } else if (num_files < 2) {
        fprintf(stderr, "install: missing destination operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    } else {
        /* Last argument is the destination */
        dst = files_arg->filename[num_files - 1];
    }

    int num_srcs = (opts.target_dir != nullptr) ? num_files : num_files - 1;
    int ret = 0;

    if (num_srcs == 1 && !opts.no_target_directory) {
        /* Check if destination is an existing directory */
        struct stat dst_stat_buf;
        if (stat(dst, &dst_stat_buf) == 0 && S_ISDIR(dst_stat_buf.st_mode)) {
            /* Single source into directory */
            const char *basename = strrchr(files_arg->filename[0], '/');
            basename = basename ? basename + 1 : files_arg->filename[0];
            char dest_path[PATH_BUF_SIZE];
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
            if (install_file(files_arg->filename[0], dest_path, &opts) != 0) {
                ret = -1;
            }
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (num_srcs > 1 && !opts.target_dir) {
        /* Multiple sources: destination must be a directory */
        struct stat dst_stat_buf;
        if (stat(dst, &dst_stat_buf) != 0 || !S_ISDIR(dst_stat_buf.st_mode)) {
            fprintf(stderr, "install: target '%s' is not a directory\n", dst);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    struct stat dst_dir_stat;
    for (int i = 0; i < num_srcs; i++) {
        const char *src = files_arg->filename[i];

        char dest_path[PATH_BUF_SIZE];
        if (opts.target_dir != nullptr || num_srcs > 1 ||
            (!opts.no_target_directory &&
             stat(dst, &dst_dir_stat) == 0 && S_ISDIR(dst_dir_stat.st_mode))) {
            /* Copy into directory: dest/basename(src) */
            const char *basename = strrchr(src, '/');
            basename = basename ? basename + 1 : src;
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
        } else {
            /* Copy to the given path */
            snprintf(dest_path, sizeof(dest_path), "%s", dst);
        }

        if (install_file(src, dest_path, &opts) != 0) {
            ret = -1;
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    (void)ret;
}
