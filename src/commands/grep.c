#include <argtable3.h>
#include <errno.h>
#include <glib.h>
#include <glib/gmacros.h>
#include <glib/gtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/grep.h"
#include "commands/search_common.h"
/** Search a single file for pattern matches. Returns the number of matching
 *  lines, or 0 if none. */
/* Maximum number of file arguments for argtable */
#define GREP_MAX_FILES 200

static int search_file(const gchar *path, gboolean is_stdin,
                        const gchar *display_name, const GrepOptions *opts,
                        GRegex *re) {
  FILE *fp;
  if (is_stdin) {
    fp = stdin;
    display_name = NULL; // no prefix for bare stdin
  } else {
    fp = fopen(path, "r");
    if (fp == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "grep: %s: %s\n", path, strerror(errno));
      return 0;
    }
  }

  int use_color = search_should_color((SearchColorMode)(int)opts->color_mode);
  int use_prefix =
      display_name != NULL &&
      (opts->always_show_filename ||
       (!opts->never_show_filename && opts->recursive));
  // If multiple files were given (detected at caller), prefix is forced too.
  // We pass prefix state via the display_name — caller sets it.

  int match_count = 0;
  int line_count = 0;
  gchar *line = NULL;
  size_t linecap = 0;

  while (getline(&line, &linecap, fp) > 0) {
    line_count++;
    // Strip trailing newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }

    gboolean matched = FALSE;

    if (opts->mode == GREP_MODE_FIXED) {
      matched = search_match_fixed(opts->pattern, line, len,
                                    opts->ignore_case,
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
      if (opts->count_only) {
        continue;
      }
      if (opts->files_with_matches) {
        // -l: just print filename once per file
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        printf("%s\n", display_name ? display_name : "(standard input)");
        match_count = 1; // signal that we found a match
        goto done;
      }
      search_print_match(line, len, opts->line_number, line_count,
                          use_prefix ? display_name : NULL, use_color, re,
                          opts->pattern, opts->only_matching,
                          opts->mode == GREP_MODE_FIXED);
    }
  }

done:
  if (opts->count_only && !opts->files_with_matches) {
    if (display_name) {
      printf("%s:", display_name);
    }
    printf("%d\n", match_count);
  }
  free(line);
  if (!is_stdin) {
    // NOLINTNEXTLINE(bugprone-unused-return-value, cert-err33-c)
    (void)fclose(fp);
  }
  return match_count;
}

/** Recursively search a directory for matching files. */
// NOLINTNEXTLINE(misc-no-recursion)
static int search_directory(const gchar *dirpath, const GrepOptions *opts,
                             GRegex *re) {
  GDir *dir = g_dir_open(dirpath, 0, NULL);
  if (dir == NULL) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "grep: %s: %s\n", dirpath, strerror(errno));
    return 0;
  }

  int total_matches = 0;
  const gchar *entry;
  while ((entry = g_dir_read_name(dir)) != NULL) {
    if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) {
      continue;
    }

    gchar *full_path = g_build_filename(dirpath, entry, NULL);
    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        // Only descend if recursive
        total_matches += search_directory(full_path, opts, re);
      } else if (S_ISREG(st.st_mode)) {
        int matches =
            search_file(full_path, FALSE, full_path, opts, re);
        if (matches > 0) {
          total_matches += matches;
        }
      }
    }
    g_free(full_path);
  }

  g_dir_close(dir);
  return total_matches;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void grep_command(gint argc, gchar **argv) {
  GrepOptions opts = {0};
  opts.mode = GREP_MODE_BASIC;
  opts.color_mode = COLOR_NEVER_GREP;

  // Handle --color= without argtable (same pattern as ls --color)
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--color") == 0) {
      argv[i] = "--color=always";
      break;
    }
  }

  struct arg_lit *extended_opt =
      arg_lit0("E", "extended-regexp", "interpret pattern as extended regex (ERE)");
  struct arg_lit *fixed_opt =
      arg_lit0("F", "fixed-strings", "interpret pattern as fixed strings");
  struct arg_lit *ignore_case_opt =
      arg_lit0("i", "ignore-case", "ignore case distinctions");
  struct arg_lit *invert_opt =
      arg_lit0("v", "invert-match", "select non-matching lines");
  struct arg_lit *line_number_opt =
      arg_lit0("n", "line-number", "print line number with output lines");
  struct arg_lit *count_opt =
      arg_lit0("c", "count", "print only a count of matching lines per file");
  struct arg_lit *recursive_opt =
      arg_lit0("r", "recursive", "read all files under directories recursively");
  struct arg_lit *recursive2_opt =
      arg_lit0("R", "dereference-recursive",
               "read all files under directories recursively (follow symlinks)");
  struct arg_lit *word_regexp_opt =
      arg_lit0("w", "word-regexp", "match only whole words");
  struct arg_lit *line_regexp_opt =
      arg_lit0("x", "line-regexp", "match only whole lines");
  struct arg_lit *only_matching_opt =
      arg_lit0("o", "only-matching", "show only matched part of line");
  struct arg_lit *files_opt =
      arg_lit0("l", "files-with-matches",
               "print only names of FILEs with selected lines");
  struct arg_lit *with_filename_opt =
      arg_lit0("H", "with-filename", "print file name with output lines");
  struct arg_lit *no_filename_opt =
      arg_lit0("h", "no-filename", "suppress file name prefix on output");
  struct arg_str *color_opt =
      arg_str0(NULL, "color", "WHEN",
               "use markers to highlight matched strings; "
               "WHEN can be 'always', 'auto', or 'never'");
  struct arg_str *pattern_opt =
      arg_str0("e", "regexp", "PATTERN",
               "use PATTERN as the pattern (protect patterns starting with -)");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *file_arg =
      arg_filen(NULL, NULL, "FILE", 0, GREP_MAX_FILES, "file to search");
  struct arg_end *end = arg_end(20);

  // NOLINTNEXTLINE(misc-use-internal-linkage)
  void *argtable[] = {extended_opt,  fixed_opt,
                      ignore_case_opt, invert_opt,     line_number_opt,
                      count_opt,       recursive_opt,  recursive2_opt,
                      word_regexp_opt, line_regexp_opt, only_matching_opt,
                      files_opt,       with_filename_opt, no_filename_opt,
                      color_opt,       pattern_opt,    help_opt,
                      file_arg,        end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... PATTERN [FILE]...\n", argv[0]);
    printf("Search for PATTERN in each FILE or standard input.\n");
    printf("\n");
    printf("Pattern selection:\n");
    printf("  -E, --extended-regexp     PATTERN is an extended regular expression (ERE)\n");
    printf("  -F, --fixed-strings       PATTERN is a set of newline-separated fixed strings\n");
    printf("  -e, --regexp=PATTERN      use PATTERN as the pattern\n");
    printf("\n");
    printf("Matching control:\n");
    printf("  -i, --ignore-case         ignore case distinctions\n");
    printf("  -v, --invert-match        select non-matching lines\n");
    printf("  -w, --word-regexp         match only whole words\n");
    printf("  -x, --line-regexp         match only whole lines\n");
    printf("\n");
    printf("Output control:\n");
    printf("  -c, --count               print only a count of selected lines per FILE\n");
    printf("  -l, --files-with-matches  print only FILE names containing matches\n");
    printf("  -n, --line-number         print line number with output lines\n");
    printf("  -o, --only-matching       show only the part of a line matching PATTERN\n");
    printf("  -H, --with-filename       print the file name for each match\n");
    printf("  -h, --no-filename         suppress the file name prefix on output\n");
    printf("      --color=WHEN          highlight matching text; WHEN can be always, auto, never\n");
    printf("\n");
    printf("Other:\n");
    printf("  -r, --recursive           search directories recursively\n");
    printf("  -R                        like -r, but follow all symlinks\n");
    printf("\n");
    printf("Exit status:\n");
    printf("  0  if a match is found\n");
    printf("  1  if no match was found\n");
    printf("  2  if an error occurred\n");
    printf("\n");
    printf("Differences from GNU grep (NOT implemented):\n");
    printf("  -A, -B, -C  context lines (after/before/around)\n");
    printf("  -P          Perl-compatible regex (PCRE)\n");
    printf("  -z, --null-data          zero-terminated lines\n");
    printf("  -b, --byte-offset        print byte offset with output\n");
    printf("  -q, --quiet              suppress all normal output\n");
    printf("  -s, --no-messages        suppress error messages\n");
    printf("  -d ACTION                how to handle directories\n");
    printf("  --include, --exclude     file-name-based filtering\n");
    printf("  -m, --max-count          stop reading after N matches\n");
    printf("  --label                  label for stdin\n");
    printf("  -Z, --null               output NUL byte after filename\n");
    printf("\n");
    printf("Note: Basic regex (default) uses GRegex (PCRE-like) syntax,\n");
    printf("not strict POSIX BRE. Use -E for explicit ERE mode.\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    exit(2); // NOLINT(misc-include-cleaner)
  }

  // --- Parse options ---
  if (extended_opt->count > 0) {
    opts.mode = GREP_MODE_EXTENDED;
  }
  if (fixed_opt->count > 0) {
    opts.mode = GREP_MODE_FIXED;
  }

  opts.ignore_case = (ignore_case_opt->count > 0);
  opts.invert_match = (invert_opt->count > 0);
  opts.line_number = (line_number_opt->count > 0);
  opts.count_only = (count_opt->count > 0);
  opts.recursive = (recursive_opt->count > 0) || (recursive2_opt->count > 0);
  opts.word_regexp = (word_regexp_opt->count > 0);
  opts.line_regexp = (line_regexp_opt->count > 0);
  opts.only_matching = (only_matching_opt->count > 0);
  opts.files_with_matches = (files_opt->count > 0);
  opts.always_show_filename = (with_filename_opt->count > 0);
  opts.never_show_filename = (no_filename_opt->count > 0);

  if (color_opt->count > 0) {
    const char *val = color_opt->sval[0];
    if (strcmp(val, "always") == 0) {
      opts.color_mode = COLOR_ALWAYS_GREP;
    } else if (strcmp(val, "auto") == 0) {
      opts.color_mode = COLOR_AUTO_GREP;
    } else if (strcmp(val, "never") == 0) {
      opts.color_mode = COLOR_NEVER_GREP;
    } else {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr,
                    "grep: invalid argument '%s' for --color\n"
                    "Valid arguments: always, auto, never\n",
                    val);
    }
  }

  // --- Get pattern ---
  const gchar *pattern = NULL;
  if (pattern_opt->count > 0) {
    pattern = pattern_opt->sval[0];
  }

  // If no -e, pattern is the first positional argument
  if (pattern == NULL && file_arg->count > 0) {
    pattern = file_arg->filename[0];
    // Shift remaining filenames down
    file_arg->count--;
    for (int i = 0; i < file_arg->count; i++) {
      file_arg->filename[i] = file_arg->filename[i + 1];
    }
  }

  if (pattern == NULL) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "grep: no pattern specified\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    exit(2); // NOLINT(misc-include-cleaner)
  }

  opts.pattern = g_strdup(pattern);

  // --- Compile regex (if not fixed mode) ---
  GRegex *re = NULL;
  GError *re_error = NULL;
  if (opts.mode != GREP_MODE_FIXED) {
    GRegexCompileFlags flags = SEARCH_REGEX_FLAGS_DEFAULT;
    if (opts.ignore_case) {
      flags |= SEARCH_REGEX_FLAGS_CASELESS;
    }
    re = search_compile_pattern(opts.pattern, opts.word_regexp,
                                 opts.line_regexp, flags, &re_error);
    if (re == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "grep: invalid pattern '%s': %s\n", pattern,
                    re_error ? re_error->message : "unknown error");
      if (re_error) {
        g_error_free(re_error);
      }
      g_free(opts.pattern);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      exit(2); // NOLINT(misc-include-cleaner)
    }
  }

  // --- Process files ---
  int total_matches = 0;
  gboolean has_files = (file_arg->count > 0);

  if (!has_files || (file_arg->count == 1 &&
                     strcmp(file_arg->filename[0], "-") == 0)) {
    // stdin mode
    total_matches +=
        search_file(NULL, TRUE, NULL, &opts, re);
  } else if (opts.recursive && has_files) {
    // Recursive mode: treat arguments as directories/files
    for (int i = 0; i < file_arg->count; i++) {
      struct stat st;
      if (stat(file_arg->filename[i], &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
          total_matches += search_directory(
              file_arg->filename[i], &opts, re);
        } else {
          total_matches += search_file(
              file_arg->filename[i], FALSE, file_arg->filename[i],
              &opts, re);
        }
      } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "grep: %s: %s\n", file_arg->filename[i],
                      strerror(errno));
      }
    }
  } else {
    // Normal mode: search files
    int force_prefix = (file_arg->count > 1) || opts.always_show_filename;
    for (int i = 0; i < file_arg->count; i++) {
      const gchar *fname = file_arg->filename[i];
      struct stat st;
      if (stat(fname, &st) == 0 && S_ISDIR(st.st_mode)) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr,
                      "grep: %s: Is a directory (use -r for recursive)\n",
                      fname);
        continue;
      }
      int matches =
          search_file(fname, FALSE,
                      force_prefix ? fname : NULL, &opts, re);
      total_matches += matches;
    }
  }

  // --- Cleanup ---
  if (re) {
    g_regex_unref(re);
  }
  g_free(opts.pattern);
  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

  // Exit with 0 if match found, 1 otherwise
  if (total_matches > 0) {
    exit(0); // NOLINT(misc-include-cleaner)
  }
  exit(1); // NOLINT(misc-include-cleaner, misc-unreachable-code)
}
