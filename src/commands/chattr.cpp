#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <ftw.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#include <cstdint>
#include <string>

#include "commands/chattr.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

/* Flag masks */
#define FLAG_APPEND         0x00000080
#define FLAG_NOATIME        0x00000100
#define FLAG_COMPRESSED     0x00000200
#define FLAG_NODUMP         0x00000400
#define FLAG_DIRSYNC        0x00000800
#define FLAG_EXTENT         0x00001000
#define FLAG_IMMUTABLE      0x00000010
#define FLAG_JOURNAL        0x00002000
#define FLAG_SECURE_DELETED 0x00004000
#define FLAG_SYNC           0x00008000
#define FLAG_NOTAIL         0x00010000
#define FLAG_TOPDIR         0x00020000
#define FLAG_UNDELETABLE    0x00040000
#define FLAG_COMPBLK        0
#define FLAG_COMPRESS_RAW   0
#define FLAG_COMPR_DIRTY    0

struct AttrEntry {
    char letter;
    unsigned int mask;
    const char *name;
};

static const AttrEntry g_attr_map[] = {
    {'a', FLAG_APPEND, "append only"},
    {'A', FLAG_NOATIME, "no atime updates"},
    {'c', FLAG_COMPRESSED, "compressed"},
    {'d', FLAG_NODUMP, "no dump"},
    {'D', FLAG_DIRSYNC, "synchronous directory updates"},
    {'e', FLAG_EXTENT, "extent format (read-only)"},
    {'i', FLAG_IMMUTABLE, "immutable"},
    {'j', FLAG_JOURNAL, "journal data"},
    {'s', FLAG_SECURE_DELETED, "secure deletion"},
    {'S', FLAG_SYNC, "synchronous updates"},
    {'t', FLAG_NOTAIL, "no tail-merging"},
    {'T', FLAG_TOPDIR, "top of directory hierarchy"},
    {'u', FLAG_UNDELETABLE, "undeletable"},
    {'x', FLAG_COMPBLK, "compression (deprecated, no-op)"},
    {'X', FLAG_COMPRESS_RAW, "compression raw (deprecated, no-op)"},
    {'Z', FLAG_COMPR_DIRTY, "compressed dirty (deprecated, no-op)"},
};

constexpr size_t ATTR_COUNT = sizeof(g_attr_map) / sizeof(g_attr_map[0]);

/* Forward declarations */
const AttrEntry* find_attr_by_letter(char letter);
bool is_read_only_attr(char letter);

/* Helper function definitions */
const AttrEntry* find_attr_by_letter(char letter) {
    for (size_t i = 0; i < ATTR_COUNT; ++i) {
        if (g_attr_map[i].letter == letter) {
            return &g_attr_map[i];
        }
    }
    return nullptr;
}

bool is_read_only_attr(char letter) {
    return (letter == 'e');
}

/* Globals */
static const ChattrOptions* chattr_glob_opts = nullptr;
static int chattr_errors = 0;

/* Ioctl wrappers */
static int get_flags(int fd, unsigned int *flags) {
    return ioctl(fd, FS_IOC_GETFLAGS, flags);
}

static int set_flags(int fd, unsigned int flags) {
    return ioctl(fd, FS_IOC_SETFLAGS, &flags);
}

static int set_version(int fd, uint64_t version) {
    return ioctl(fd, FS_IOC_SETVERSION, &version);
}

/* Single-file operation */
static int chattr_apply_file(const char *path, unsigned int add_mask,
                            unsigned int remove_mask, bool set_exact,
                            unsigned int keep_flags, const ChattrOptions *opts,
                            bool use_version, const char *version_str) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        if (!opts->is_silent) {
            fprintf(stderr, "chattr: cannot open '%s': %s\n", path, strerror(errno));
        }
        return 1;
    }

    /* Handle version number first */
    if (use_version && version_str) {
        uint64_t version = std::stoull(version_str);
        if (set_version(fd, version) < 0) {
            close(fd);
            if (!opts->is_silent) {
                fprintf(stderr, "chattr: set version on '%s': %s\n", path, strerror(errno));
            }
            return 1;
        }
        if (opts->is_verbose || opts->is_recursive) {
            printf("changed generation number of '%s' to %lu\n", path, version);
        }
        close(fd);
        return 0;
    }

    unsigned int current_flags = 0;
    if (get_flags(fd, &current_flags) < 0) {
        close(fd);
        if (!opts->is_silent) {
            fprintf(stderr, "chattr: cannot get flags for '%s': %s\n", path, strerror(errno));
        }
        close(fd);
        return 1;
    }

    unsigned int new_flags = current_flags;

    if (set_exact) {
        unsigned int attr_mask = 0;
        for (size_t i = 0; i < ATTR_COUNT; ++i) {
            if (!is_read_only_attr(g_attr_map[i].letter)) {
                attr_mask |= g_attr_map[i].mask;
            }
        }
        new_flags &= ~attr_mask;
        new_flags |= keep_flags;
    } else {
        new_flags |= add_mask;
        new_flags &= ~remove_mask;
    }

    if (new_flags != current_flags) {
        if (set_flags(fd, new_flags) < 0) {
            close(fd);
            if (!opts->is_silent) {
                fprintf(stderr, "chattr: set flags on '%s': %s\n", path, strerror(errno));
            }
            close(fd);
            return 1;
        }
        if (opts->is_verbose || opts->is_recursive) {
            printf("changed attributes of '%s'\n", path);
        }
    }

    close(fd);
    return 0;
}

/* Recursive callback */
static int recursive_callback(const char *fpath, const struct stat *sb,
                              int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (chattr_glob_opts->preserve_root && strcmp(fpath, "/") == 0) {
        fprintf(stderr, "chattr: it is dangerous to operate recursively on '/'\n");
        fprintf(stderr, "chattr: use --no-preserve-root to override this failsafe\n");
        chattr_errors = 1;
        return 0;
    }

    if (typeflag == FTW_NS || typeflag == FTW_DNR || typeflag == FTW_SLN) {
        if (!chattr_glob_opts->is_silent) {
            fprintf(stderr, "chattr: cannot access '%s': %s\n", fpath, strerror(errno));
        }
        chattr_errors = 1;
        return 0;
    }

    if (chattr_apply_file(fpath, 0, 0, false, 0, chattr_glob_opts, false, nullptr) != 0) {
        chattr_errors = 1;
    }
    return 0;
}

/* Help output */
static void print_help(const char *prog) {
    printf("Usage: %s [OPTION]... MODE FILE...\n", prog);
    printf("Change attributes of each FILE to MODE.\n");
    printf("\n");
    printf("  -R, --recursive        change files and directories recursively\n");
    printf("  -V, --verbose          output a diagnostic for every file processed\n");
    printf("  -f, --suppress         suppress most error messages\n");
    printf("      --preserve-root    fail to operate recursively on '/'\n");
    printf("      --no-preserve-root do not treat '/' specially (the default)\n");
    printf("      --help             display this help and exit\n");
    printf("\n");
    printf("Each MODE is of the form [+][aAcCdDeijsStTu].\n");
    printf("\n");
    printf("Supported attributes:\n");
    for (size_t i = 0; i < ATTR_COUNT; ++i) {
        printf("  %c - %s (%s)", g_attr_map[i].letter, g_attr_map[i].name,
               (g_attr_map[i].letter == 'e' ? "(read-only)" : ""));
        if (i < ATTR_COUNT - 1) printf(", ");
        printf("\n");
    }
}

/* Command entry point */
int chattr_command(int argc, char **argv) {
    struct arg_lit *recursive_opt = arg_lit0("R", "recursive",
        "change files and directories recursively");
    struct arg_lit *verbose_opt = arg_lit0("v", "verbose",
        "output a diagnostic for every file processed");
    struct arg_lit *silent_opt = arg_litn("f", "suppress", 0, 1,
        "suppress most error messages");
    struct arg_str *version_num_opt = arg_str0(NULL, "version", "VERSION",
        "Set the file's version/generation number");
    struct arg_lit *preserve_root_opt = arg_lit0(NULL, "preserve-root",
        "fail to operate recursively on '/'");
    struct arg_lit *no_preserve_root_opt = arg_lit0(NULL, "no-preserve-root",
        "do not treat '/' specially (the default)");
    struct arg_lit *help_opt = arg_lit0("h", "help",
        "display this help and exit");
    struct arg_file *all_args = arg_filen(NULL, NULL, "MODE FILE...", 0, 1000,
        "mode and file(s) to change");
    struct arg_end *end = arg_end(20);

    ArgTable at({recursive_opt, verbose_opt, silent_opt, version_num_opt,
                 preserve_root_opt, no_preserve_root_opt, help_opt, all_args, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_help(argv[0]);
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    std::vector<std::string> mode_strings;
    std::vector<std::string> file_paths;
    int version_set = 0;
    const char *ver_str = nullptr;

    if (version_num_opt->count > 0) {
        ver_str = version_num_opt->sval[0];
        version_set = 1;
    }

    int num_tokens = all_args->count;
    if (num_tokens == 0) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    for (int i = 0; i < num_tokens; ++i) {
        const char *token = all_args->filename[i];
        if (*token == '+' || *token == '-' || *token == '=') {
            mode_strings.push_back(token);
        } else {
            file_paths.push_back(token);
        }
    }

    if (file_paths.empty()) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    if (mode_strings.empty() && !version_set) {
        fprintf(stderr, "%s: missing mode specification\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    ChattrOptions opts = {};
    opts.recursive_opt = recursive_opt;
    opts.verbose_opt = verbose_opt;
    opts.silent_opt = silent_opt;
    opts.version_num_opt = version_num_opt;
    opts.preserve_root_opt = preserve_root_opt;
    opts.no_preserve_root_opt = no_preserve_root_opt;
    opts.help_opt = help_opt;
    opts.files_arg = all_args;
    opts.is_recursive = recursive_opt->count > 0;
    opts.is_verbose = verbose_opt->count > 0;
    opts.is_silent = silent_opt->count > 0;
    opts.preserve_root = preserve_root_opt->count > 0;
    opts.no_preserve_root = no_preserve_root_opt->count > 0;
    opts.use_version = version_set;
    opts.version_str = version_set ? ver_str : nullptr;

    unsigned int add_mask = 0;
    unsigned int remove_mask = 0;
    bool set_exact = false;
    unsigned int keep_flags = 0;

    if (!mode_strings.empty()) {
        for (const auto &mode_str : mode_strings) {
            char op = *mode_str.c_str();
            const char *p = mode_str.c_str() + 1;

            if (op != '+' && op != '-' && op != '=') {
                fprintf(stderr, "%s: invalid mode: '%s' — must start with +, -, or =\n",
                        argv[0], mode_str.c_str());
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            }

            while (*p) {
                char letter = *p++;
                const AttrEntry* entry = find_attr_by_letter(letter);
                if (!entry) {
                    fprintf(stderr, "%s: unknown attribute '%c'\n", argv[0], letter);
                    fprintf(stderr, "Valid attributes: ");
                    for (size_t i = 0; i < ATTR_COUNT; ++i) {
                        printf("%c ", g_attr_map[i].letter);
                    }
                    printf("\n");
                    return 1;
                }

                if (is_read_only_attr(letter)) {
                    if (op == '-') {
                        fprintf(stderr, "%s: read-only attribute '%c' cannot be removed\n",
                                argv[0], letter);
                        return 1;
                    }
                    add_mask |= entry->mask;
                    if (op == '=') {
                        keep_flags |= entry->mask;
                    }
                    continue;
                }

                if (op == '+') {
                    add_mask |= entry->mask;
                } else if (op == '-') {
                    remove_mask |= entry->mask;
                } else if (op == '=') {
                    keep_flags |= entry->mask;
                    set_exact = true;
                }

                if (*p == ',') p++;
            }
        }
    }

    if (opts.is_recursive) {
        chattr_glob_opts = &opts;

        for (size_t fi = 0; fi < file_paths.size(); ++fi) {
            const char *path = file_paths[fi].c_str();
            if (nftw(path, recursive_callback, 20, FTW_PHYS) != 0) {
                if (!opts.is_silent) {
                    fprintf(stderr, "chattr: '%s': %s\n", path, strerror(errno));
                }
                chattr_errors = 1;
            }
        }
    } else {
        for (size_t fi = 0; fi < file_paths.size(); ++fi) {
            const char *path = file_paths[fi].c_str();
            if (chattr_apply_file(path, add_mask, remove_mask, set_exact,
                                 keep_flags, &opts,
                                 opts.use_version, opts.version_str) != 0) {
                chattr_errors = 1;
            }
        }
    }

    return chattr_errors ? 1 : 0;
}

REGISTER_COMMAND("chattr", chattr_command, "Change file attributes on a Linux file system")