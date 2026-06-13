#include <argtable3.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/rg.h"
#include "commands/search_common.h"

/* Maximum number of file arguments for argtable */
#define RG_MAX_FILES 200

/* Maximum glob patterns */
#define RG_MAX_GLOBS 50

/** Check if a filename matches any glob pattern (fnmatch-style). */
static int match_glob(const gchar *filename, GPtrArray *globs) {
    if (globs == NULL || globs->len == 0) {
        return 1; // no globs = match all
    }

    for (guint i = 0; i < globs->len; i++) {
        const gchar *pat = (const gchar *)g_ptr_array_index(globs, i);
        // NOLINTNEXTLINE(misc-include-cleaner)
        gboolean exclude = FALSE;
        const gchar *actual_pat = pat;

        if (pat[0] == '!') {
            // NOLINTNEXTLINE(misc-include-cleaner)
            exclude = TRUE;
            actual_pat = pat + 1;
        }

        // Simple fnmatch-style matching using GPatternSpec
        GPatternSpec *spec = g_pattern_spec_new(actual_pat);
        gboolean matched = g_pattern_spec_match_string(spec, filename);
        g_pattern_spec_free(spec);

        if (matched) {
            return exclude ? 0 : 1;
        }
    }

    // If we only have exclude patterns, non-matching files are included
    int has_include = 0;
    for (guint i = 0; i < globs->len; i++) {
        const gchar *pat = (const gchar *)g_ptr_array_index(globs, i);
        if (pat[0] != '!') {
            has_include = 1;
            break;
        }
    }
    return has_include ? 0 : 1;
}

/** Check if a file/directory should be considered hidden. */
static int is_hidden(const gchar *name) {
    return name[0] == '.';
}

/** Determine if pattern is all-lowercase (for smart-case). */
static int is_pattern_lowercase(const gchar *pattern) {
    for (const gchar *p = pattern; *p; p++) {
        if (g_ascii_isupper((gchar)*p)) {
            return 0;
        }
    }
    return 1;
}

/** Determine if case-insensitive match should be used. */
static gboolean rg_should_ci(const RgOptions *opts) {
    if (opts->case_sensitive) {
        return FALSE;
    }
    if (opts->ignore_case) {
        return TRUE;
    }
    if (opts->smart_case) {
        return is_pattern_lowercase(opts->pattern);
    }
    return FALSE;
}

/** Print context separator. */
static void rg_print_context_sep(void) {
    printf("--\n");
}

/** Resolve context lines: prefer specific (-A/-B), fall back to -C. */
static int resolve_context(int specific, int fallback) {
    if (specific > 0) {
        return specific;
    }
    if (fallback > 0) {
        return fallback;
    }
    return 0;
}

/** Search a single file. Returns match count. */
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static int rg_search_file(const gchar *path, gboolean is_stdin,
                           const gchar *display_name, const RgOptions *opts,
                           GRegex *re, gboolean is_fixed, gboolean is_ci) {
    FILE *fp;
    if (is_stdin) {
        fp = stdin;
        display_name = NULL;
    } else {
        fp = fopen(path, "r");
        if (fp == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "rg: %s: %s\n", path, strerror(errno));
            return 0;
        }
    }

    int use_color = search_should_color((SearchColorMode)(int)opts->color_mode);
    int show_ln = opts->line_number && !opts->no_line_number;

    /* Determine if we need a filename prefix:
     * - Always if -H was passed
     * - If more than one file/dir was searched (determined by caller)
     * We receive prefix info through display_name being set. */
    int use_prefix = (display_name != NULL);
    // Context tracking
    int match_count = 0;
    int line_count = 0;
    gchar *line = NULL;
    size_t linecap = 0;

    // Context output buffer: we need to track matches for -C/-A/-B
    // Simple approach: store recent lines for before-context
    int context_before = resolve_context(opts->context_before, opts->context);
    int context_after = resolve_context(opts->context_after, opts->context);

    // Ring buffer for before-context lines
    gchar **before_lines = NULL;
    int *before_line_nums = NULL;
    int before_head = 0;
    int before_count = 0;
    if (context_before > 0) {
        before_lines = g_new0(gchar *, (guint)context_before);
        before_line_nums = g_new0(int, (guint)context_before);
    }

    int pending_after = 0; // lines of after-context remaining to print
    int had_match_before = 0; // whether we've printed any match yet (for -- separator)
    gboolean in_match = FALSE;

    while (getline(&line, &linecap, fp) > 0) {
        line_count++;
        // Strip trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        gboolean matched = FALSE;
        if (is_fixed) {
            matched = search_match_fixed(opts->pattern, line, len,
                                          is_ci,
                                          opts->word_regexp,
                                          opts->line_regexp);
        } else {
            matched = g_regex_match(re, line, (GRegexMatchFlags)0, NULL);
        }

        if (opts->invert_match) {
            matched = !matched;
        }

        if (matched) {
            match_count++;

            // In count-only, files-with-matches, or only-matching mode, skip separator
            int skip_output = opts->count_only || opts->files_with_matches
                              || opts->only_matching;

            // Flush before-context buffer (if any)
            if (context_before > 0 && before_count > 0) {
                if (!skip_output) {
                    gboolean first_group = (had_match_before == 0);
                    if (!first_group && !in_match) {
                        rg_print_context_sep();
                    }
                    // Print stored before-context lines
                    for (int i = 0; i < before_count; i++) {
                        int idx = (before_head - before_count + i) %
                                  (context_before > 0 ? context_before : 1);
                        if (idx < 0) {
                            idx += (context_before > 0 ? context_before : 1);
                        }
                        if (before_lines[idx]) {
                            if (use_prefix) {
                                printf("%s:", display_name);
                            }
                            if (show_ln) {
                                printf("%d-", before_line_nums[idx]);
                            }
                            printf("%s\n", before_lines[idx]);
                        }
                    }
                }
                before_count = 0;
            } else if (had_match_before && !in_match && !skip_output) {
                // Separator between match groups
                rg_print_context_sep();
            }

            in_match = TRUE;
            had_match_before = 1;

            if (opts->count_only) {
                // In count mode, just increment and continue
                if (opts->max_count > 0 && match_count >= opts->max_count) {
                    goto cleanup;
                }
                continue;
            }
            if (opts->files_with_matches) {
                printf("%s\n", display_name ? display_name : "(standard input)");
                match_count = 1;
                goto cleanup;
            }

            search_print_match(line, len, show_ln, line_count,
                               use_prefix ? display_name : NULL,
                               use_color, re, opts->pattern,
                               opts->only_matching, is_fixed);

            pending_after = context_after;

            if (opts->max_count > 0 && match_count >= opts->max_count) {
                goto cleanup;
            }
        } else if (pending_after > 0) {
            if (use_prefix) {
                printf("%s:", display_name);
            }
            if (show_ln) {
                printf("%d-", line_count);
            }
            printf("%s\n", line);
            pending_after--;
            if (pending_after == 0) {
                in_match = FALSE; // end of after-context group
            }
            // Reset before-context tracking since we just showed context
            before_count = 0;
        } else {
            in_match = FALSE;
            // Store in before-context buffer (if tracking)
            if (context_before > 0) {
                if (before_lines[before_head]) {
                    g_free(before_lines[before_head]);
                }
                before_lines[before_head] = g_strdup(line);
                before_line_nums[before_head] = line_count;
                before_head = (before_head + 1) % context_before;
                if (before_count < context_before) {
                    before_count++;
                }
            }
        }
    }

cleanup:
    if (opts->count_only && !opts->files_with_matches) {
        if (display_name) {
            printf("%s:", display_name);
        }
        printf("%d\n", match_count);
    }

    free(line);
    if (before_lines) {
        for (int i = 0; i < context_before; i++) {
            g_free(before_lines[i]);
        }
        g_free((gpointer)before_lines);
        g_free((gpointer)before_line_nums);
    }
    if (!is_stdin) { // NOLINT(clang-analyzer-unix.Malloc)
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)fclose(fp);
    }
    return match_count;
}

/** Recursively search a directory. Returns total match count. */
// NOLINTNEXTLINE(misc-no-recursion)
static int rg_search_directory(const gchar *dirpath, const RgOptions *opts,
                                GRegex *re, gboolean is_fixed,
                                gboolean is_ci, int depth) {
    if (opts->max_depth >= 0 && depth > opts->max_depth) {
        return 0;
    }

    GDir *dir = g_dir_open(dirpath, 0, NULL);
    if (dir == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "rg: %s: %s\n", dirpath, strerror(errno));
        return 0;
    }

    int total_matches = 0;
    const gchar *entry;
    while ((entry = g_dir_read_name(dir)) != NULL) {
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) {
            continue;
        }

        // Hidden file handling
        if (!opts->hidden && is_hidden(entry)) {
            continue;
        }

        gchar *full_path = g_build_filename(dirpath, entry, NULL);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            // Glob filter
            if (!match_glob(entry, opts->glob_patterns)) {
                g_free(full_path);
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                total_matches += rg_search_directory(
                    full_path, opts, re, is_fixed, is_ci, depth + 1);
            } else if (S_ISREG(st.st_mode)) {
                total_matches += rg_search_file(
                    full_path, FALSE, full_path, opts, re, is_fixed, is_ci);
            }
        }
        g_free(full_path);
    }

    g_dir_close(dir);
    return total_matches;
}

/** Check if a path is a directory. */
static int rg_is_directory(const gchar *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void rg_command(gint argc, gchar **argv) {
    RgOptions opts = {0};
    opts.mode = RG_MODE_BASIC;
    opts.color_mode = RG_COLOR_NEVER;
    opts.line_number = 1;     // default: on
    opts.smart_case = 1;      // default: on
    opts.max_depth = -1;      // unlimited

    // Handle --color= without argtable
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--color") == 0) {
            argv[i] = "--color=always";
            break;
        }
    }

    struct arg_lit *extended_opt =
        arg_lit0("E", "extended-regexp",
                 "interpret pattern as extended regex (ERE)");
    struct arg_lit *fixed_opt =
        arg_lit0("F", "fixed-strings",
                 "interpret pattern as fixed strings");
    struct arg_lit *ignore_case_opt =
        arg_lit0("i", "ignore-case", "case-insensitive matching");
    struct arg_lit *smart_case_opt =
        arg_lit0("S", "smart-case",
                 "case-insensitive if pattern is all lowercase (default)");
    struct arg_lit *case_sensitive_opt =
        arg_lit0("s", "case-sensitive", "force case-sensitive matching");
    struct arg_lit *invert_opt =
        arg_lit0("v", "invert-match", "select non-matching lines");
    struct arg_lit *line_number_opt =
        arg_lit0("n", "line-number", "show line numbers (default)");
    struct arg_lit *no_line_number_opt =
        arg_lit0("N", "no-line-number", "suppress line numbers");
    struct arg_lit *count_opt =
        arg_lit0("c", "count", "print only a count of matches per file");
    struct arg_lit *word_regexp_opt =
        arg_lit0("w", "word-regexp", "match only whole words");
    struct arg_lit *line_regexp_opt =
        arg_lit0("x", "line-regexp", "match only whole lines");
    struct arg_lit *only_matching_opt =
        arg_lit0("o", "only-matching", "show only matched part of line");
    struct arg_lit *files_opt =
        arg_lit0("l", "files-with-matches",
                 "print only names of files with matches");
    struct arg_lit *hidden_opt =
        arg_lit0(NULL, "hidden", "search hidden files and directories");
    struct arg_int *max_count_opt =
        arg_int0("m", "max-count", "NUM",
                 "stop after NUM matches per file");
    struct arg_int *max_depth_opt =
        arg_int0(NULL, "max-depth", "NUM",
                 "descend at most NUM directories deep");
    struct arg_int *context_opt =
        arg_int0("C", "context", "NUM",
                 "show NUM lines before and after each match");
    struct arg_int *after_opt =
        arg_int0("A", "after-context", "NUM",
                 "show NUM lines after each match");
    struct arg_int *before_opt =
        arg_int0("B", "before-context", "NUM",
                 "show NUM lines before each match");
    struct arg_str *glob_opt =
        arg_strn("g", "glob", "GLOB", 0, RG_MAX_GLOBS,
                 "glob pattern to include/exclude files (!prefix to exclude)");
    struct arg_str *color_opt =
        arg_str0(NULL, "color", "WHEN",
                 "use markers to highlight matches; "
                 "WHEN can be 'always', 'auto', or 'never'");
    struct arg_str *pattern_opt =
        arg_str0("e", "regexp", "PATTERN",
                 "use PATTERN as the pattern (protect patterns starting with -)");
    struct arg_lit *help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_file *file_arg =
        arg_filen(NULL, NULL, "PATH", 0, RG_MAX_FILES,
                  "file or directory to search");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        extended_opt,       fixed_opt,
        ignore_case_opt,    smart_case_opt,    case_sensitive_opt,
        invert_opt,         line_number_opt,   no_line_number_opt,
        count_opt,          word_regexp_opt,   line_regexp_opt,
        only_matching_opt,  files_opt,         hidden_opt,
        max_count_opt,      max_depth_opt,
        context_opt,        after_opt,         before_opt,
        glob_opt,           color_opt,         pattern_opt,
        help_opt,           file_arg,          end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTIONS] PATTERN [PATH...]\n", argv[0]);
        printf("Recursively search files for PATTERN (like ripgrep).\n");
        printf("\n");
        printf("Pattern selection:\n");
        printf("  -e, --regexp=PATTERN  use PATTERN as the pattern\n");
        printf("  -F, --fixed-strings   treat PATTERN as literal string\n");
        printf("  -E, --extended-regexp treat PATTERN as extended regex\n");
        printf("\n");
        printf("Matching control:\n");
        printf("  -i, --ignore-case     case-insensitive matching\n");
        printf("  -s, --case-sensitive  force case-sensitive matching\n");
        printf("  -S, --smart-case      case-insensitive if pattern is all lowercase\n");
        printf("  -v, --invert-match    select non-matching lines\n");
        printf("  -w, --word-regexp     match only whole words\n");
        printf("  -x, --line-regexp     match only whole lines\n");
        printf("\n");
        printf("Output control:\n");
        printf("  -n, --line-number     show line numbers (default)\n");
        printf("  -N, --no-line-number  suppress line numbers\n");
        printf("  -c, --count           count matches per file\n");
        printf("  -l, --files-with-matches  print only matching file names\n");
        printf("  -o, --only-matching   show only matched text\n");
        printf("      --color=WHEN      highlight matches; WHEN=always/auto/never\n");
        printf("\n");
        printf("Context:\n");
        printf("  -C, --context=NUM     show NUM lines around each match\n");
        printf("  -A, --after-context=NUM  show NUM lines after each match\n");
        printf("  -B, --before-context=NUM show NUM lines before each match\n");
        printf("\n");
        printf("Filtering:\n");
        printf("  -g, --glob=GLOB       glob pattern (!prefix to exclude)\n");
        printf("      --hidden          search hidden files and directories\n");
        printf("      --max-depth=NUM   max directory recursion depth\n");
        printf("  -m, --max-count=NUM   stop after NUM matches per file\n");
        printf("\n");
        printf("Exit status:\n");
        printf("  0  if a match is found\n");
        printf("  1  if no match was found\n");
        printf("  2  if an error occurred\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        exit(2);
    }

    // --- Parse options ---
    if (extended_opt->count > 0) {
        opts.mode = RG_MODE_EXTENDED;
    }
    if (fixed_opt->count > 0) {
        opts.mode = RG_MODE_FIXED;
        opts.fixed_strings = 1;
    }

    opts.ignore_case = (ignore_case_opt->count > 0);
    if (smart_case_opt->count > 0) {
        opts.smart_case = 1;
    }
    opts.case_sensitive = (case_sensitive_opt->count > 0);
    opts.invert_match = (invert_opt->count > 0);
    if (line_number_opt->count > 0) {
        opts.line_number = 1;
    }
    if (no_line_number_opt->count > 0) {
        opts.no_line_number = 1;
    }
    opts.count_only = (count_opt->count > 0);
    opts.word_regexp = (word_regexp_opt->count > 0);
    opts.line_regexp = (line_regexp_opt->count > 0);
    opts.only_matching = (only_matching_opt->count > 0);
    opts.files_with_matches = (files_opt->count > 0);
    opts.hidden = (hidden_opt->count > 0);

    if (max_count_opt->count > 0) {
        opts.max_count = max_count_opt->ival[0];
    }
    if (max_depth_opt->count > 0) {
        opts.max_depth = max_depth_opt->ival[0];
    }
    if (context_opt->count > 0) {
        opts.context = context_opt->ival[0];
    }
    if (after_opt->count > 0) {
        opts.context_after = after_opt->ival[0];
    }
    if (before_opt->count > 0) {
        opts.context_before = before_opt->ival[0];
    }

    if (glob_opt->count > 0) {
        opts.glob_patterns = g_ptr_array_new();
        for (int i = 0; i < glob_opt->count; i++) {
            g_ptr_array_add(opts.glob_patterns,
                            (gpointer)g_strdup(glob_opt->sval[i]));
        }
    }

    if (color_opt->count > 0) {
        const char *val = color_opt->sval[0];
        if (strcmp(val, "always") == 0) {
            opts.color_mode = RG_COLOR_ALWAYS;
        } else if (strcmp(val, "auto") == 0) {
            opts.color_mode = RG_COLOR_AUTO;
        } else if (strcmp(val, "never") == 0) {
            opts.color_mode = RG_COLOR_NEVER;
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr,
                          "rg: invalid argument '%s' for --color\n"
                          "Valid arguments: always, auto, never\n",
                          val);
        }
    }

    // Override smart_case if -i or -s was explicit
    if (opts.ignore_case || opts.case_sensitive) {
        opts.smart_case = 0;
    }

    // --- Get pattern ---
    const gchar *pattern = NULL;
    if (pattern_opt->count > 0) {
        pattern = pattern_opt->sval[0];
    }

    if (pattern == NULL && file_arg->count > 0) {
        pattern = file_arg->filename[0];
        file_arg->count--;
        for (int i = 0; i < file_arg->count; i++) {
            file_arg->filename[i] = file_arg->filename[i + 1];
        }
    }

    if (pattern == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "rg: no pattern specified\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        exit(2);
    }

    opts.pattern = g_strdup(pattern);

    gboolean is_ci = rg_should_ci(&opts);
    gboolean is_fixed = (opts.mode == RG_MODE_FIXED);

    // --- Compile regex (if not fixed mode) ---
    GRegex *re = NULL;
    GError *re_error = NULL;
    if (!is_fixed) {
        GRegexCompileFlags flags = SEARCH_REGEX_FLAGS_DEFAULT;
        if (opts.case_sensitive) {
            // force case-sensitive: use default flags only
        } else if (opts.ignore_case ||
                   (opts.smart_case && is_pattern_lowercase(opts.pattern))) {
            flags |= SEARCH_REGEX_FLAGS_CASELESS;
        }
        re = search_compile_pattern(opts.pattern, opts.word_regexp,
                                     opts.line_regexp, flags, &re_error);
        if (re == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "rg: invalid pattern '%s': %s\n",
                          pattern,
                          re_error ? re_error->message : "unknown error");
            if (re_error) {
                g_error_free(re_error);
            }
            g_free(opts.pattern);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            exit(2);
        }
    }

    // --- Process files/directories ---
    int total_matches = 0;
    gboolean has_paths = (file_arg->count > 0);
    int has_dirs = 0;

    // First pass: classify arguments
    if (has_paths) {
        for (int i = 0; i < file_arg->count; i++) {
            if (strcmp(file_arg->filename[i], "-") == 0 ||
                rg_is_directory(file_arg->filename[i])) {
                has_dirs = 1;
            }
        }
    }

    int force_prefix = (has_paths && (file_arg->count > 1 || has_dirs));
    // In auto-recursive mode, always show prefix for files under directories
    if (has_dirs) {
        force_prefix = 1;
    }

    if (!has_paths ||
        (file_arg->count == 1 &&
         strcmp(file_arg->filename[0], "-") == 0)) {
        // stdin mode
        total_matches += rg_search_file(
            NULL, TRUE, NULL, &opts, re, is_fixed, is_ci);
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            const gchar *fname = file_arg->filename[i];
            struct stat st;

            if (strcmp(fname, "-") == 0) {
                // Explicit stdin
                total_matches += rg_search_file(
                    NULL, TRUE, NULL, &opts, re, is_fixed, is_ci);
                continue;
            }

            if (stat(fname, &st) != 0) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "rg: %s: %s\n", fname, strerror(errno));
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                // Auto-recursive
                total_matches += rg_search_directory(
                    fname, &opts, re, is_fixed, is_ci, 0);
            } else if (S_ISREG(st.st_mode)) {
                total_matches += rg_search_file(
                    fname, FALSE,
                    force_prefix ? fname : NULL,
                    &opts, re, is_fixed, is_ci);
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr,
                              "rg: %s: not a regular file or directory\n",
                              fname);
            }
        }
    }

    // --- Cleanup ---
    if (re) {
        g_regex_unref(re);
    }
    g_free(opts.pattern);
    if (opts.glob_patterns) {
        g_ptr_array_free(opts.glob_patterns, TRUE);
    }
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (total_matches > 0) {
        exit(0);
    }
    exit(1);
}
