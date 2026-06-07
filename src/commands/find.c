#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/find.h"
/* ── Constants ────────────────────────────────────────────────────── */

#define FIND_MAX_DEPTH_UNLIMITED (-1)
#define EXIT_EXEC_FAIL 127
#define IS_PATH_SEP(c) ((c) == '/')

/* ── Forward declarations ────────────────────────────────────────── */

static int  find_evaluate(const gchar *fullpath, const gchar *basename,
                          const struct stat *st, const FindOptions *opts,
                          int depth);
static int  find_walk(const gchar *dirpath, FindOptions *opts, int depth);
static void find_exec_file(const gchar *fullpath, FindOptions *opts);
static void find_exec_finalize(FindOptions *opts);
static void find_usage(const gchar *progname);

/* ── Predicate helpers ───────────────────────────────────────────── */

/** Check if a filename matches a GPatternSpec glob pattern. */
static int glob_match(const gchar *pattern, const gchar *name) {
    static GPatternSpec *cached_spec = NULL;
    static gchar *cached_pattern = NULL;

    if (cached_pattern == NULL || strcmp(cached_pattern, pattern) != 0) {
        g_free(cached_pattern);
        if (cached_spec != NULL) {
            g_pattern_spec_free(cached_spec);
        }
        cached_pattern = g_strdup(pattern);
        cached_spec = g_pattern_spec_new(pattern);
    }

    return (int)g_pattern_spec_match_string(cached_spec, name);
}

/** Check if a file/directory is empty. For regular files: size == 0.
 *  For directories: contains no entries (except . and ..). */
static int is_empty_path(const gchar *fullpath, const struct stat *st) {
    if (S_ISREG(st->st_mode)) {
        return st->st_size == 0;
    }
    if (S_ISDIR(st->st_mode)) {
        DIR *dir = opendir(fullpath);
        if (dir == NULL) {
            return 0;
        }
        int empty = 1;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 &&
                strcmp(entry->d_name, "..") != 0) {
                empty = 0;
                break;
            }
        }
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)closedir(dir);
        return empty;
    }
    return 0;
}

/* ── Expression evaluation ───────────────────────────────────────── */

/** Evaluate all predicates for a single file entry.
 *  Returns 1 if all predicates match, 0 otherwise. */
static int find_evaluate(const gchar *fullpath, const gchar *basename,
                         const struct stat *st, const FindOptions *opts,
                         int depth) {
    // Depth constraints
    if (opts->max_depth != FIND_MAX_DEPTH_UNLIMITED && depth > opts->max_depth) {
        return 0;
    }
    if (depth < opts->min_depth) {
        return 0;
    }

    // -name
    if (opts->name_pattern != NULL) {
        if (!glob_match(opts->name_pattern, basename)) {
            return 0;
        }
    }

    // -iname
    if (opts->iname_pattern != NULL) {
        gchar *lower_pat = g_utf8_strdown(opts->iname_pattern, -1);
        gchar *lower_name = g_utf8_strdown(basename, -1);
        int matched = glob_match(lower_pat, lower_name);
        g_free(lower_name);
        g_free(lower_pat);
        if (!matched) {
            return 0;
        }
    }

    // -type
    if (opts->type_filter != 0) {
        switch (opts->type_filter) {
        case 'f':
            if (!S_ISREG(st->st_mode)) { return 0; }
            break;
        case 'd':
            if (!S_ISDIR(st->st_mode)) { return 0; }
            break;
        case 'l':
            // We always lstat, so st reflects the symlink itself, not its target
            if (!S_ISLNK(st->st_mode)) { return 0; }
            break;
        default:
            break;
        }
    }

    // -empty
    if (opts->empty_only) {
        if (!is_empty_path(fullpath, st)) {
            return 0;
        }
    }

    return 1;
}

/* ── Tree walking ────────────────────────────────────────────────── */

/** Recursively walk a directory and process entries.
 *  Returns 0 on success, -1 on error. */
// NOLINTNEXTLINE(misc-no-recursion)
static int find_walk(const gchar *dirpath, FindOptions *opts, int depth) {
    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "find: %s: %s\n", dirpath, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        gchar *fullpath = g_build_filename(dirpath, entry->d_name, NULL);

        // lstat to detect symlinks
        struct stat st;
        if (lstat(fullpath, &st) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "find: %s: %s\n", fullpath, strerror(errno));
            g_free(fullpath);
            continue;
        }

        // Evaluate predicates
        if (find_evaluate(fullpath, entry->d_name, &st, opts, depth + 1)) {
            find_exec_file(fullpath, opts);
        }

        // Recurse into directories (follow symlinks to dirs?)
        if (S_ISDIR(st.st_mode) &&
            (opts->max_depth == FIND_MAX_DEPTH_UNLIMITED ||
             depth + 1 <= opts->max_depth)) {
            find_walk(fullpath, opts, depth + 1);
        }

        g_free(fullpath);
    }

    (void)closedir(dir); // NOLINTNEXTLINE(bugprone-unused-return-value)
    return 0;
}

/* ── Actions ─────────────────────────────────────────────────────── */

/** Perform actions (print, delete) for a matching file. */
static void find_exec_file(const gchar *fullpath, FindOptions *opts) {
    // -print
    if (opts->do_print) {
        printf("%s\n", fullpath);
    }

    // -delete
    if (opts->do_delete) {
        struct stat st;
        if (lstat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (rmdir(fullpath) != 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: cannot delete '%s': %s\n",
                              fullpath, strerror(errno));
            }
        } else {
            if (unlink(fullpath) != 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: cannot delete '%s': %s\n",
                              fullpath, strerror(errno));
            }
        }
    }

    // -exec
    if (opts->has_exec) {
        if (opts->exec_plus) {
            // Accumulate for batch execution
            g_ptr_array_add(opts->exec_paths, g_strdup(fullpath));
        } else {
            // Per-file execution
            // NOLINTNEXTLINE(misc-include-cleaner)
            pid_t pid = fork();
            if (pid == 0) {
                // Child: build args with {} substitution
                GPtrArray *args = g_ptr_array_new();
                for (guint i = 0; i < opts->exec_args->len; i++) {
                    const gchar *arg = (const gchar *)g_ptr_array_index(opts->exec_args, i);
                    if (strcmp(arg, "{}") == 0) {
                        g_ptr_array_add(args, (gpointer)fullpath);
                    } else {
                        g_ptr_array_add(args, (gpointer)arg);
                    }
                }
                g_ptr_array_add(args, NULL);
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)execvp((const gchar *)g_ptr_array_index(args, 0),
                             (char *const *)args->pdata);
                // execvp failed
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: %s: %s\n",
                              (const gchar *)g_ptr_array_index(args, 0),
                              strerror(errno));
                g_ptr_array_free(args, TRUE); // NOLINT(misc-include-cleaner)
                _exit(EXIT_EXEC_FAIL);
            } else if (pid > 0) {
                int status;
                // NOLINTNEXTLINE(bugprone-unused-return-value)
                (void)waitpid(pid, &status, 0);
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: fork failed: %s\n", strerror(errno));
            }
        }
    }
}

/** Finalize exec+ by running the command once with all accumulated paths. */
static void find_exec_finalize(FindOptions *opts) {
    if (!opts->has_exec || !opts->exec_plus) {
        return;
    }
    if (opts->exec_paths->len == 0) {
        return;
    }
    // NOLINTNEXTLINE(misc-include-cleaner)
    pid_t pid = fork();
    if (pid == 0) {
        GPtrArray *args = g_ptr_array_new();
        for (guint i = 0; i < opts->exec_args->len; i++) {
            const gchar *arg = (const gchar *)g_ptr_array_index(opts->exec_args, i);
            if (strcmp(arg, "{}") == 0) {
                // Add all accumulated paths
                for (guint j = 0; j < opts->exec_paths->len; j++) {
                    g_ptr_array_add(args,
                                    g_ptr_array_index(opts->exec_paths, j));
                }
            } else {
                g_ptr_array_add(args, (gpointer)arg);
            }
        }
        g_ptr_array_add(args, NULL);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)execvp((const gchar *)g_ptr_array_index(args, 0),
                     (char *const *)args->pdata);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "find: %s: %s\n",
                      (const gchar *)g_ptr_array_index(args, 0),
                      strerror(errno));
        g_ptr_array_free(args, TRUE); // NOLINT(misc-include-cleaner)
        _exit(EXIT_EXEC_FAIL);
    } else if (pid > 0) {
        int status;
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)waitpid(pid, &status, 0);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "find: fork failed: %s\n", strerror(errno));
    }
}

/* ── Argument parsing ────────────────────────────────────────────── */
/**
 * Parse find expression from argv.
 * Expression starts after the first non-flag, non-recognized position
 * that doesn't look like a predicate. In GNU find, paths come first,
 * then predicates.
 *
 * We use a simple heuristic: collect leading non-`-` args as paths,
 * then everything else as expression predicates, until we hit `--`
 * (end of options marker).
 *
 * Returns 0 on success, -1 on error (caller should exit). */
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static int find_parse_args(int argc, gchar **argv, FindOptions *opts,
                           int *help_requested) {
    int i;
    int collecting_paths = 1;

    opts->max_depth = FIND_MAX_DEPTH_UNLIMITED;
    opts->min_depth = 0;

    for (i = 1; i < argc; i++) {
        const gchar *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            *help_requested = 1;
            return 0;
        }
        if (strcmp(arg, "--") == 0) {
            for (int j = i + 1; j < argc; j++) {
                g_ptr_array_add(opts->paths, g_strdup(argv[j]));
            }
            break;
        }

        if (collecting_paths) {
            if (arg[0] == '-') {
                collecting_paths = 0;
            } else {
                g_ptr_array_add(opts->paths, g_strdup(arg));
                continue;
            }
        }

        // ── Expression predicates ──
        if (strcmp(arg, "-name") == 0) {
            if (i + 1 >= argc) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: missing argument to `-name'\n");
                return -1;
            }
            i++;
            if (opts->name_pattern) {
                g_free(opts->name_pattern);
            }
            opts->name_pattern = g_strdup(argv[i]);
        } else if (strcmp(arg, "-iname") == 0) {
            if (i + 1 >= argc) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: missing argument to `-iname'\n");
                return -1;
            }
            i++;
            if (opts->iname_pattern) {
                g_free(opts->iname_pattern);
            }
            opts->iname_pattern = g_strdup(argv[i]);
        } else if (strcmp(arg, "-type") == 0) {
            if (i + 1 >= argc) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: missing argument to `-type'\n");
                return -1;
            }
            i++;
            const gchar *type_str = argv[i];
            if (type_str[0] == '\0' || type_str[1] != '\0' ||
                (type_str[0] != 'f' && type_str[0] != 'd' && type_str[0] != 'l')) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr,
                              "find: unknown argument to `-type': %s\n",
                              type_str);
                return -1;
            }
            opts->type_filter = type_str[0];
        } else if (strcmp(arg, "-maxdepth") == 0) {
            if (i + 1 >= argc) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: missing argument to `-maxdepth'\n");
                return -1;
            }
            i++;
            char *end;
            long val = strtol(argv[i], &end, 10);
            if (*end != '\0' || val < 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr,
                              "find: invalid argument `%s' to `-maxdepth'\n",
                              argv[i]);
                return -1;
            }
            opts->max_depth = (int)val;
        } else if (strcmp(arg, "-mindepth") == 0) {
            if (i + 1 >= argc) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: missing argument to `-mindepth'\n");
                return -1;
            }
            i++;
            char *end;
            long val = strtol(argv[i], &end, 10);
            if (*end != '\0' || val < 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr,
                              "find: invalid argument `%s' to `-mindepth'\n",
                              argv[i]);
                return -1;
            }
            opts->min_depth = (int)val;
        } else if (strcmp(arg, "-empty") == 0) {
            opts->empty_only = 1;
        } else if (strcmp(arg, "-print") == 0) {
            opts->do_print = 1;
            opts->has_action = 1;
        } else if (strcmp(arg, "-delete") == 0) {
            opts->do_delete = 1;
            opts->has_action = 1;
        } else if (strcmp(arg, "-exec") == 0) {
            if (i + 1 >= argc) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "find: missing argument to `-exec'\n");
                return -1;
            }
            opts->has_exec = 1;
            opts->has_action = 1;
            opts->exec_args = g_ptr_array_new();
            i++;
            gboolean found_terminator = FALSE; // NOLINT(misc-include-cleaner)
            while (i < argc) {
                if (strcmp(argv[i], ";") == 0) {
                    found_terminator = TRUE;
                    break;
                }
                if (strcmp(argv[i], "+") == 0) {
                    opts->exec_plus = 1;
                    found_terminator = TRUE;
                    break;
                }
                g_ptr_array_add(opts->exec_args, g_strdup(argv[i]));
                i++;
            }
            if (!found_terminator) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr,
                              "find: missing terminating `;' or `+' for `-exec'\n");
                return -1;
            }
            if (opts->exec_paths == NULL) {
                opts->exec_paths = g_ptr_array_new_with_free_func(g_free);
            }
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "find: unknown predicate `%s'\n", arg);
            return -1;
        }
    }

    return 0;
}

/* ── Usage / Help ────────────────────────────────────────────────── */

static void find_usage(const gchar *progname) {
    printf("Usage: %s [starting-point...] [expression]\n", progname);
    printf("Search for files in a directory hierarchy.\n");
    printf("\n");
    printf("Predicates:\n");
    printf("  -name PATTERN      shell glob pattern matching on filename\n");
    printf("  -iname PATTERN     case-insensitive -name\n");
    printf("  -type [fdl]        file is of type f (regular), d (directory), l (symlink)\n");
    printf("  -empty             file is empty (regular file or directory)\n");
    printf("\n");
    printf("Numeric options:\n");
    printf("  -maxdepth N        descend at most N levels below starting points\n");
    printf("  -mindepth N        do not apply tests/actions at levels less than N\n");
    printf("\n");
    printf("Actions:\n");
    printf("  -print             print file path (default if no action specified)\n");
    printf("  -delete            delete file or empty directory\n");
    printf("  -exec cmd {} ;     execute command on each matching file ({} replaced)\n");
    printf("  -exec cmd {} +     execute command with all matching files at once\n");
    printf("\n");
    printf("If no starting point is given, the current directory `.` is used.\n");
    printf("If no action is given, `-print` is assumed.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -name \"*.c\"\n", progname);
    printf("  %s . -type f -name \"*.txt\"\n", progname);
    printf("  %s /tmp -empty -delete\n", progname);
    printf("  %s . -type f -exec wc -l {} ;\n", progname);
    printf("  %s . -name \"*.log\" -exec rm {} +\n", progname);
    printf("  %s -maxdepth 2 -type d -name \"test*\"\n", progname);
}

/* ── Entry point ─────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-size)
void find_command(gint argc, gchar **argv) {
    FindOptions opts = {0};
    opts.paths = g_ptr_array_new_with_free_func(g_free);
    opts.exec_paths = g_ptr_array_new_with_free_func(g_free);

    int help_requested = 0;
    if (find_parse_args(argc, argv, &opts, &help_requested) != 0) {
        if (opts.paths) {
            g_ptr_array_unref(opts.paths);
        }
        if (opts.exec_paths) {
            g_ptr_array_unref(opts.exec_paths);
        }
        exit(1);
    }

    if (help_requested) {
        find_usage(argv[0]);
        g_ptr_array_unref(opts.paths);
        g_ptr_array_unref(opts.exec_paths);
        return;
    }

    // Default: print if no action specified
    if (!opts.has_action) {
        opts.do_print = 1;
    }

    // Default starting point
    if (opts.paths->len == 0) {
        g_ptr_array_add(opts.paths, g_strdup("."));
    }

    // Walk each starting point
    for (guint i = 0; i < opts.paths->len; i++) {
        const gchar *start = (const gchar *)g_ptr_array_index(opts.paths, i);
        struct stat st;

        if (lstat(start, &st) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "find: '%s': %s\n", start, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Start directory: evaluate at depth 0 (the dir itself, not its contents)
            // and then recurse with depth 0 for contents
            if (find_evaluate(start, start, &st, &opts, 0)) {
                find_exec_file(start, &opts);
            }
            // Recurse into children with depth 1
            if (opts.max_depth == FIND_MAX_DEPTH_UNLIMITED || opts.max_depth > 0) {
                find_walk(start, &opts, 0);
            }
        } else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            if (find_evaluate(start, start, &st, &opts, 0)) {
                find_exec_file(start, &opts);
            }
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "find: '%s': unknown file type\n", start);
        }
    }

    // Finalize exec+
    find_exec_finalize(&opts);

    // Cleanup
    if (opts.name_pattern) {
        g_free(opts.name_pattern);
    }
    if (opts.iname_pattern) {
        g_free(opts.iname_pattern);
    }
    if (opts.exec_args) {
        for (guint i = 0; i < opts.exec_args->len; i++) {
            g_free(g_ptr_array_index(opts.exec_args, i));
        }
        g_ptr_array_unref(opts.exec_args);
    }
    if (opts.exec_paths) {
        g_ptr_array_unref(opts.exec_paths);
    }
    g_ptr_array_unref(opts.paths);
}
