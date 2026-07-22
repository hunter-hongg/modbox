#include <argtable3.h>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fnmatch.h>
#include <optional>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/rg.hpp"
#include "commands/search_common.hpp"
#include "commands/command_macros.hpp"

/* Maximum number of file arguments for argtable */
#define RG_MAX_FILES 200

/* Maximum glob patterns */
#define RG_MAX_GLOBS 50

/** Check if a filename matches any glob pattern (fnmatch-style). */
static int match_glob(const char* filename,
                      const std::vector<std::string>& globs) {
    if (globs.empty()) {
        return 1; // no globs = match all
    }

    for (std::size_t i = 0; i < globs.size(); i++) {
        const std::string& pat = globs[i];
        bool exclude = false;
        const char* actual_pat = pat.c_str();

        if (actual_pat[0] == '!') {
            exclude = true;
            actual_pat = actual_pat + 1;
        }

        int matched = (fnmatch(actual_pat, filename, FNM_PATHNAME) == 0);

        if (matched) {
            return exclude ? 0 : 1;
        }
    }

    // If we only have exclude patterns, non-matching files are included
    int has_include = 0;
    for (std::size_t i = 0; i < globs.size(); i++) {
        if (globs[i][0] != '!') {
            has_include = 1;
            break;
        }
    }
    return has_include ? 0 : 1;
}

/** Check if a file/directory should be considered hidden. */
static int is_hidden(const char* name) {
    return name[0] == '.';
}

/** Determine if pattern is all-lowercase (for smart-case). */
static int is_pattern_lowercase(const char* pattern) {
    for (const char* p = pattern; *p; p++) {
        if (std::isupper((unsigned char)*p)) {
            return 0;
        }
    }
    return 1;
}

/** Determine if case-insensitive match should be used. */
static bool rg_should_ci(const RgOptions* opts) {
    if (opts->case_sensitive) {
        return false;
    }
    if (opts->ignore_case) {
        return true;
    }
    if (opts->smart_case) {
        return is_pattern_lowercase(opts->pattern.c_str()) != 0;
    }
    return false;
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
static int rg_search_file(const char* path, bool is_stdin,
                           const char* display_name, const RgOptions* opts,
                           const std::regex* re, bool is_fixed, bool is_ci) {
    FILE* fp;
    if (is_stdin) {
        fp = stdin;
        display_name = nullptr;
    } else {
        fp = fopen(path, "r");
        if (fp == nullptr) {
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
    int use_prefix = (display_name != nullptr);
    // Context tracking
    int match_count = 0;
    int line_count = 0;
    char* line = nullptr;
    size_t linecap = 0;

    // Context output buffer: we need to track matches for -C/-A/-B
    // Simple approach: store recent lines for before-context
    int context_before = resolve_context(opts->context_before, opts->context);
    int context_after = resolve_context(opts->context_after, opts->context);

    // Ring buffer for before-context lines
    std::vector<std::string> before_lines_vec;
    std::vector<int> before_line_nums_vec;
    int before_head = 0;
    int before_count = 0;
    if (context_before > 0) {
        before_lines_vec.resize((std::size_t)context_before);
        before_line_nums_vec.resize((std::size_t)context_before);
    }

    int pending_after = 0; // lines of after-context remaining to print
    int had_match_before = 0; // whether we've printed any match yet (for -- separator)
    bool in_match = false;

    while (getline(&line, &linecap, fp) > 0) {
        line_count++;
        // Strip trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        bool matched = false;
        if (is_fixed) {
            matched = search_match_fixed(opts->pattern.c_str(), line, len,
                                          is_ci,
                                          opts->word_regexp,
                                          opts->line_regexp);
        } else if (opts->word_regexp) {
            // Manually check word boundaries
            std::string s(line, len);
            std::smatch m;
            auto search_start = s.cbegin();
            while (std::regex_search(search_start, s.cend(), m, *re)) {
                std::size_t abs_pos = (std::size_t)(m.position(0) + (search_start - s.cbegin()));
                if (search_check_word_boundary(line, abs_pos, abs_pos + m.length(0), len)) {
                    matched = true;
                    break;
                }
                search_start = m.suffix().first;
                if (search_start == s.cend()) break;
            }
        } else {
            matched = std::regex_search(line, *re);
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
                    bool first_group = (had_match_before == 0);
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
                        if (!before_lines_vec[(std::size_t)idx].empty()) {
                            if (use_prefix) {
                                printf("%s:", display_name);
                            }
                            if (show_ln) {
                                printf("%d-", before_line_nums_vec[(std::size_t)idx]);
                            }
                            printf("%s\n", before_lines_vec[(std::size_t)idx].c_str());
                        }
                    }
                }
                before_count = 0;
            } else if (had_match_before && !in_match && !skip_output) {
                // Separator between match groups
                rg_print_context_sep();
            }

            in_match = true;
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
                               use_prefix ? display_name : nullptr,
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
                in_match = false; // end of after-context group
            }
            // Reset before-context tracking since we just showed context
            before_count = 0;
        } else {
            in_match = false;
            // Store in before-context buffer (if tracking)
            if (context_before > 0) {
                before_lines_vec[(std::size_t)before_head] = line;
                before_line_nums_vec[(std::size_t)before_head] = line_count;
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
    if (!is_stdin) { // NOLINT(clang-analyzer-unix.Malloc)
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)fclose(fp);
    }
    return match_count;
}

/** Recursively search a directory. Returns total match count. */
// NOLINTNEXTLINE(misc-no-recursion)
static int rg_search_directory(const char* dirpath, const RgOptions* opts,
                                const std::regex* re, bool is_fixed,
                                bool is_ci, int depth) {
    if (opts->max_depth >= 0 && depth > opts->max_depth) {
        return 0;
    }

    std::error_code ec;
    std::filesystem::path dir(dirpath);

    int total_matches = 0;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "rg: %s: %s\n", dirpath, strerror(errno));
            return total_matches;
        }

        std::string filename = entry.path().filename().string();
        if (filename == "." || filename == "..") {
            continue;
        }

        // Hidden file handling
        if (!opts->hidden && is_hidden(filename.c_str())) {
            continue;
        }

        std::string full_path = entry.path().string();
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            // Glob filter
            if (!match_glob(filename.c_str(), opts->glob_patterns)) {
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                total_matches += rg_search_directory(
                    full_path.c_str(), opts, re, is_fixed, is_ci, depth + 1);
            } else if (S_ISREG(st.st_mode)) {
                total_matches += rg_search_file(
                    full_path.c_str(), false, full_path.c_str(), opts, re, is_fixed, is_ci);
            }
        }
    }

    return total_matches;
}

/** Check if a path is a directory. */
static int rg_is_directory(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void rg_command(int argc, char** argv) {
    RgOptions opts;
    opts.mode = RgMode::BASIC;
    opts.color_mode = RgColor::AUTO;
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

    struct arg_lit* extended_opt =
        arg_lit0("E", "extended-regexp",
                 "interpret pattern as extended regex (ERE)");
    struct arg_lit* fixed_opt =
        arg_lit0("F", "fixed-strings",
                 "interpret pattern as fixed strings");
    struct arg_lit* ignore_case_opt =
        arg_lit0("i", "ignore-case", "case-insensitive matching");
    struct arg_lit* smart_case_opt =
        arg_lit0("S", "smart-case",
                 "case-insensitive if pattern is all lowercase (default)");
    struct arg_lit* case_sensitive_opt =
        arg_lit0("s", "case-sensitive", "force case-sensitive matching");
    struct arg_lit* invert_opt =
        arg_lit0("v", "invert-match", "select non-matching lines");
    struct arg_lit* line_number_opt =
        arg_lit0("n", "line-number", "show line numbers (default)");
    struct arg_lit* no_line_number_opt =
        arg_lit0("N", "no-line-number", "suppress line numbers");
    struct arg_lit* count_opt =
        arg_lit0("c", "count", "print only a count of matches per file");
    struct arg_lit* word_regexp_opt =
        arg_lit0("w", "word-regexp", "match only whole words");
    struct arg_lit* line_regexp_opt =
        arg_lit0("x", "line-regexp", "match only whole lines");
    struct arg_lit* only_matching_opt =
        arg_lit0("o", "only-matching", "show only matched part of line");
    struct arg_lit* files_opt =
        arg_lit0("l", "files-with-matches",
                 "print only names of files with matches");
    struct arg_lit* hidden_opt =
        arg_lit0(nullptr, "hidden", "search hidden files and directories");
    struct arg_int* max_count_opt =
        arg_int0("m", "max-count", "NUM",
                 "stop after NUM matches per file");
    struct arg_int* max_depth_opt =
        arg_int0(nullptr, "max-depth", "NUM",
                 "descend at most NUM directories deep");
    struct arg_int* context_opt =
        arg_int0("C", "context", "NUM",
                 "show NUM lines before and after each match");
    struct arg_int* after_opt =
        arg_int0("A", "after-context", "NUM",
                 "show NUM lines after each match");
    struct arg_int* before_opt =
        arg_int0("B", "before-context", "NUM",
                 "show NUM lines before each match");
    struct arg_str* glob_opt =
        arg_strn("g", "glob", "GLOB", 0, RG_MAX_GLOBS,
                 "glob pattern to include/exclude files (!prefix to exclude)");
    struct arg_str* color_opt =
        arg_str0(nullptr, "color", "WHEN",
                 "use markers to highlight matches; "
                 "WHEN can be 'always', 'auto', or 'never'");
    struct arg_str* pattern_opt =
        arg_str0("e", "regexp", "PATTERN",
                 "use PATTERN as the pattern (protect patterns starting with -)");
    struct arg_lit* help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg =
        arg_filen(nullptr, nullptr, "PATH", 0, RG_MAX_FILES,
                  "file or directory to search");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
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
        opts.mode = RgMode::EXTENDED;
    }
    if (fixed_opt->count > 0) {
        opts.mode = RgMode::FIXED;
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
        for (int i = 0; i < glob_opt->count; i++) {
            opts.glob_patterns.push_back(glob_opt->sval[i]);
        }
    }

    if (color_opt->count > 0) {
        const char* val = color_opt->sval[0];
        if (strcmp(val, "always") == 0) {
            opts.color_mode = RgColor::ALWAYS;
        } else if (strcmp(val, "auto") == 0) {
            opts.color_mode = RgColor::AUTO;
        } else if (strcmp(val, "never") == 0) {
            opts.color_mode = RgColor::NEVER;
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
    const char* pattern = nullptr;
    if (pattern_opt->count > 0) {
        pattern = pattern_opt->sval[0];
    }

    if (pattern == nullptr && file_arg->count > 0) {
        pattern = file_arg->filename[0];
        file_arg->count--;
        for (int i = 0; i < file_arg->count; i++) {
            file_arg->filename[i] = file_arg->filename[i + 1];
        }
    }

    if (pattern == nullptr) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "rg: no pattern specified\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        exit(2);
    }

    opts.pattern = pattern;

    bool is_ci = rg_should_ci(&opts);
    bool is_fixed = (opts.mode == RgMode::FIXED);

    // --- Compile regex (if not fixed mode) ---
    std::optional<std::regex> re_opt;
    const std::regex* re = nullptr;
    if (!is_fixed) {
        auto re_flags = std::regex::optimize;
        if (opts.case_sensitive) {
            // force case-sensitive: use default flags only
        } else if (opts.ignore_case ||
                   (opts.smart_case && is_pattern_lowercase(opts.pattern.c_str()))) {
            re_flags |= std::regex::icase;
        }
        try {
            re_opt.emplace(search_compile_pattern(opts.pattern, opts.word_regexp,
                                                   opts.line_regexp, re_flags));
            re = &*re_opt;
        } catch (const std::regex_error& e) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "rg: invalid pattern '%s': %s\n",
                          pattern, e.what());
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            exit(2);
        }
    }

    // --- Process files/directories ---
    int total_matches = 0;
    bool has_paths = (file_arg->count > 0);
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
            nullptr, true, nullptr, &opts, re, is_fixed, is_ci);
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            const char* fname = file_arg->filename[i];
            struct stat st;

            if (strcmp(fname, "-") == 0) {
                // Explicit stdin
                total_matches += rg_search_file(
                    nullptr, true, nullptr, &opts, re, is_fixed, is_ci);
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
                    fname, false,
                    force_prefix ? fname : nullptr,
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
    // regex and glob_patterns destructors handle cleanup
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (total_matches > 0) {
        exit(0);
    }
    exit(1);
}

REGISTER_COMMAND("rg", rg_command, "Alias for grep --color=always -r");
