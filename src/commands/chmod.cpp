#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/chmod.hpp"

#define PERM_MASK 07777

/* ── Mode flags ────────────────────────────────────────────────────────── */

#define MODE_ABSOLUTE 0
#define MODE_SYMBOLIC 1

/* ── Symbolic mode helpers ─────────────────────────────────────────────── */

#define WHO_NONE 0
#define WHO_U    (1 << 0)
#define WHO_G    (1 << 1)
#define WHO_O    (1 << 2)
#define WHO_A    (WHO_U | WHO_G | WHO_O)

static mode_t who_mask_bits(int who) {
    mode_t m = 0;
    if (who & WHO_U) m |= S_IRWXU;
    if (who & WHO_G) m |= S_IRWXG;
    if (who & WHO_O) m |= S_IRWXO;
    return m;
}

static int parse_who(const char **pp) {
    int who = WHO_NONE;
    while (**pp) {
        if (**pp == 'u') { who |= WHO_U; (*pp)++; }
        else if (**pp == 'g') { who |= WHO_G; (*pp)++; }
        else if (**pp == 'o') { who |= WHO_O; (*pp)++; }
        else if (**pp == 'a') { who |= WHO_A; (*pp)++; }
        else { break; }
    }
    return who ? who : WHO_A;
}

static void parse_perm_part(const char **pp, mode_t *perm, mode_t current,
                            int who_mask) {
    while (**pp) {
        switch (**pp) {
            case 'r':
                if (who_mask & WHO_U) *perm |= S_IRUSR;
                if (who_mask & WHO_G) *perm |= S_IRGRP;
                if (who_mask & WHO_O) *perm |= S_IROTH;
                (*pp)++; break;
            case 'w':
                if (who_mask & WHO_U) *perm |= S_IWUSR;
                if (who_mask & WHO_G) *perm |= S_IWGRP;
                if (who_mask & WHO_O) *perm |= S_IWOTH;
                (*pp)++; break;
            case 'x':
                if (who_mask & WHO_U) *perm |= S_IXUSR;
                if (who_mask & WHO_G) *perm |= S_IXGRP;
                if (who_mask & WHO_O) *perm |= S_IXOTH;
                (*pp)++; break;
            case 'X':
                if (S_ISDIR(current) || (current & (S_IXUSR | S_IXGRP | S_IXOTH)))
                    *perm |= who_mask_bits(who_mask & (WHO_U | WHO_G | WHO_O)) & (S_IXUSR | S_IXGRP | S_IXOTH);
                (*pp)++; break;
            case 's':
                if (who_mask & WHO_U) *perm |= S_ISUID;
                if (who_mask & WHO_G) *perm |= S_ISGID;
                (*pp)++; break;
            case 't':
                *perm |= S_ISVTX;
                (*pp)++; break;
            case 'u':
                if (who_mask & WHO_U) *perm |= (current & S_IRWXU);
                (*pp)++; break;
            case 'g':
                if (who_mask & WHO_G) *perm |= (current & S_IRWXG);
                (*pp)++; break;
            case 'o':
                if (who_mask & WHO_O) *perm |= (current & S_IRWXO);
                (*pp)++; break;
            default: return;
        }
    }
}

static mode_t apply_symbolic_clause(const char *clause, mode_t current) {
    const char *p = clause;
    int who = parse_who(&p);
    mode_t result = current;
    while (*p) {
        char op = *p;
        if (op != '+' && op != '-' && op != '=') break;
        p++;
        mode_t perm = 0;
        parse_perm_part(&p, &perm, current, who);
        if (op == '+') {
            result |= perm;
        } else if (op == '-') {
            result &= ~perm;
        } else if (op == '=') {
            result &= ~who_mask_bits(who);
            result |= perm;
        }
        if (*p == ',') { p++; who = parse_who(&p); }
    }
    return result;
}

static int parse_symbolic_mode(const char *mode_str, mode_t *out) {
    char tmp[256];
    strncpy(tmp, mode_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *save = nullptr;
    const char *tok = strtok_r(tmp, ",", &save);
    if (!tok) return -1;
    mode_t result = *out;
    while (tok) {
        result = apply_symbolic_clause(tok, result);
        tok = strtok_r(nullptr, ",", &save);
    }
    *out = result;
    return 0;
}

/* ── Mode classification ───────────────────────────────────────────────── */

static int classify_and_parse_mode(const char *mode_str, mode_t *out) {
    char *endptr = nullptr;
    long m = strtol(mode_str, &endptr, 8);
    if (*endptr == '\0' && m >= 0 && m <= PERM_MASK) {
        *out = (mode_t)(m & PERM_MASK);
        return MODE_ABSOLUTE;
    }
    if (*mode_str == '\0') return -1;
    if (*mode_str != '+' && *mode_str != '-' && *mode_str != '=' &&
        *mode_str != 'u' && *mode_str != 'g' && *mode_str != 'o' && *mode_str != 'a')
        return -1;
    mode_t dummy = 0;
    if (parse_symbolic_mode(mode_str, &dummy) != 0) return -1;
    *out = 0;
    return MODE_SYMBOLIC;
}

/* ── File-scope globals for nftw callback ──────────────────────────────── */

static const ChmodOptions *chmod_glob_opts;
static const char *chmod_mode_str;
static int chmod_errors;
static int chmod_changes_made;

/* ── Single-file chmod ─────────────────────────────────────────────────── */

static int chmod_one_file(const char *path, const ChmodOptions *opts,
                          int mode_kind) {
    struct stat cur_stat;
    if (stat(path, &cur_stat) != 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chmod: cannot access '%s': %s\n",
                    path, strerror(errno));
        }
        return 1;
    }

    mode_t old_mode = cur_stat.st_mode & PERM_MASK;
    mode_t new_mode = old_mode;

    if (opts->reference) {
        struct stat ref_stat;
        if (stat(opts->reference, &ref_stat) != 0) {
            if (!opts->is_silent) {
                fprintf(stderr, "chmod: cannot access '%s': %s\n",
                        opts->reference, strerror(errno));
            }
            return 1;
        }
        new_mode = ref_stat.st_mode & PERM_MASK;
    } else if (mode_kind == MODE_ABSOLUTE) {
        new_mode = opts->mode;
    } else {
        if (parse_symbolic_mode(chmod_mode_str, &new_mode) != 0) {
            if (!opts->is_silent) {
                fprintf(stderr, "chmod: invalid mode: '%s'\n", chmod_mode_str);
            }
            return 1;
        }
    }

    if (new_mode == old_mode) return 0;

    if (chmod(path, new_mode) != 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chmod: changing permissions of '%s': %s\n",
                    path, strerror(errno));
        }
        return 1;
    }

    chmod_changes_made = 1;

    if (opts->is_verbose || opts->is_changes) {
        char old_buf[16], new_buf[16];
        snprintf(old_buf, sizeof(old_buf), "0%03o", (unsigned)old_mode);
        snprintf(new_buf, sizeof(new_buf), "0%03o", (unsigned)new_mode);
        printf("mode of '%s' changed from %s (%04o) to %s (%04o)\n",
               path, old_buf, (unsigned)old_mode, new_buf, (unsigned)new_mode);
    }
    return 0;
}

/* ── Recursive nftw callback ───────────────────────────────────────────── */

static int recursive_callback(const char *fpath, const struct stat *sb,
                               int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (typeflag == FTW_NS || typeflag == FTW_DNR || typeflag == FTW_SLN) {
        if (!chmod_glob_opts->is_silent) {
            fprintf(stderr, "chmod: cannot access '%s': %s\n",
                    fpath, strerror(errno));
        }
        chmod_errors = 1;
        return 0;
    }

    if (chmod_glob_opts->preserve_root && strcmp(fpath, "/") == 0) {
        if (!chmod_glob_opts->is_silent) {
            fprintf(stderr, "chmod: it is dangerous to operate recursively on '/'\n");
            fprintf(stderr, "chmod: use --no-preserve-root to override this failsafe\n");
        }
        chmod_errors = 1;
        return 0;
    }

    int mode_kind = chmod_mode_str ? MODE_SYMBOLIC : MODE_ABSOLUTE;
    if (chmod_one_file(fpath, chmod_glob_opts, mode_kind) != 0) {
        chmod_errors = 1;
    }
    return 0;
}



/* ── Command entry point ───────────────────────────────────────────────── */

void chmod_command(int argc, char **argv) {
    struct arg_lit *recursive_opt =
        arg_lit0("R", "recursive", "change files and directories recursively");
    struct arg_lit *verbose_opt =
        arg_lit0("v", "verbose", "output a diagnostic for every file processed");
    struct arg_lit *changes_opt =
        arg_lit0("c", "changes", "like verbose but report only when a change is made");
    struct arg_lit *silent_opt =
        arg_litn("f", "silent", 0, 1, "suppress most error messages");
    struct arg_lit *quiet_opt =
        arg_lit0(NULL, "quiet", "suppress most error messages");
    struct arg_str *reference_opt =
        arg_str0(NULL, "reference", "RFILE", "use RFILE's mode instead of MODE values");
    struct arg_lit *preserve_root_opt =
        arg_lit0(NULL, "preserve-root", "fail to operate recursively on '/'");
    struct arg_lit *no_preserve_root_opt =
        arg_lit0(NULL, "no-preserve-root", "do not treat '/' specially (the default)");
    struct arg_lit *help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_file *all_args =
        arg_filen(NULL, NULL, "MODE FILE...", 0, 1000, "mode and files to change");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {recursive_opt, verbose_opt, changes_opt, silent_opt,
                         quiet_opt, reference_opt, preserve_root_opt,
                         no_preserve_root_opt, help_opt, all_args, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... MODE[,MODE]... FILE...\n", argv[0]);
        printf("  or:  %s [OPTION]... OCTAL-MODE FILE...\n", argv[0]);
        printf("  or:  %s [OPTION]... --reference=RFILE FILE...\n", argv[0]);
        printf("Change the mode of each FILE to MODE.\n");
        printf("With --reference, change the mode of each FILE to that of RFILE.\n");
        printf("\n");
        printf("  -c, --changes          like verbose but report only when a change is made\n");
        printf("  -f, --silent, --quiet  suppress most error messages\n");
        printf("  -v, --verbose          output a diagnostic for every file processed\n");
        printf("      --no-preserve-root do not treat '/' specially (the default)\n");
        printf("      --preserve-root    fail to operate recursively on '/'\n");
        printf("      --reference=RFILE  use RFILE's mode instead of MODE values\n");
        printf("  -R, --recursive        change files and directories recursively\n");
        printf("      --help             display this help and exit\n");
        printf("\n");
        printf("Each MODE is of the form '[ugoa]*([-+=]([rwxXst]*|[ugo]))+'.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int mode_is_set = 0;
    const char *mode_str = nullptr;
    int num_files = all_args->count;
    int file_offset = 0;

    if (reference_opt->count == 0) {
        /* First positional arg is the mode */
        if (num_files < 1) {
            fprintf(stderr, "%s: missing operand\n", argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        mode_str = all_args->filename[0];
        mode_is_set = 1;
        file_offset = 1;
        num_files--;
    }

    if (num_files == 0) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }


    ChmodOptions opts = {};
    opts.is_recursive = (recursive_opt->count > 0);
    opts.is_verbose = (verbose_opt->count > 0);
    opts.is_changes = (changes_opt->count > 0);
    opts.is_silent = (silent_opt->count > 0 || quiet_opt->count > 0);
    opts.preserve_root = (preserve_root_opt->count > 0);
    opts.reference = (reference_opt->count > 0) ? reference_opt->sval[0] : nullptr;

    int mode_kind = -1;
    if (reference_opt->count > 0) {
        opts.mode_set = 1;
        mode_kind = MODE_ABSOLUTE;
    } else if (mode_is_set) {
        mode_t parsed = 0;
        mode_kind = classify_and_parse_mode(mode_str, &parsed);
        if (mode_kind < 0) {
            fprintf(stderr, "%s: invalid mode: '%s'\n", argv[0], mode_str);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        if (mode_kind == MODE_ABSOLUTE) {
            opts.mode = parsed & PERM_MASK;
        }
        opts.mode_set = 1;
    }

    chmod_errors = 0;
    chmod_changes_made = 0;

    if (opts.is_recursive) {
        chmod_glob_opts = &opts;
        chmod_mode_str = (mode_kind == MODE_SYMBOLIC) ? mode_str : nullptr;

        for (int i = 0; i < num_files; i++) {
            const char *path = all_args->filename[file_offset + i];
            if (nftw(path, recursive_callback, 20, FTW_PHYS) != 0) {
                if (!opts.is_silent) {
                    fprintf(stderr, "chmod: %s: %s\n", path, strerror(errno));
                }
                chmod_errors = 1;
            }
        }
    } else {
        chmod_mode_str = (mode_kind == MODE_SYMBOLIC) ? mode_str : nullptr;
        for (int i = 0; i < num_files; i++) {
            const char *path = all_args->filename[file_offset + i];
            if (chmod_one_file(path, &opts, mode_kind) != 0) {
                chmod_errors = 1;
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
