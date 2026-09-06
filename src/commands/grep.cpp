#include "argtable3.h"
#include "commands/arg_util.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fnmatch.h>
#include <optional>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <vector>

#include "commands/grep.hpp"
#include "commands/grep_tui.hpp"
#include "commands/search_common.hpp"
#include "commands/command_macros.hpp"

/** Search a single file for pattern matches. Returns the number of matching
 *  lines, or 0 if none. */
/* Maximum number of file arguments for argtable */
#define GREP_MAX_FILES 200

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static int search_file(const char* path, bool is_stdin,
                       const char* display_name, const GrepOptions* opts,
                       const std::regex* re) {
  FILE* fp;
  if (is_stdin) {
    fp = stdin;
    display_name = nullptr; // no prefix for bare stdin
  } else {
    fp = fopen(path, "r");
    if (fp == nullptr) {
      if (opts->no_messages == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "grep: %s: %s\n", path, strerror(errno));
      }
      return 0;
    }
  }

  int const use_color = search_should_color(static_cast<SearchColorMode>(static_cast<int>(opts->color_mode)));
  int const use_prefix =
      static_cast<int>(display_name != nullptr &&
      ((opts->always_show_filename != 0) ||
       ((opts->never_show_filename == 0) && (opts->recursive != 0))));
  // If multiple files were given (detected at caller), prefix is forced too.
  // We pass prefix state via the display_name — caller sets it.

  int match_count = 0;
  int line_count = 0;
  char* line = nullptr;
  size_t linecap = 0;

  while (getline(&line, &linecap, fp) > 0) {
    line_count++;
    // Strip trailing newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }

    bool matched = false;

    if (opts->mode == GrepMode::FIXED) {
      matched = search_match_fixed(opts->pattern.c_str(), line, len,
                                    opts->ignore_case,
                                    opts->word_regexp,
                                    opts->line_regexp);
    } else if (opts->word_regexp != 0) {
      // Manually check word boundaries since std::regex doesn't support lookahead/lookbehind
      std::string const s(line, len);
      std::smatch m;
      auto search_start = s.cbegin();
      while (std::regex_search(search_start, s.cend(), m, *re)) {
        std::size_t const abs_pos = static_cast<std::size_t>(m.position(0) + (search_start - s.cbegin()));
        if (search_check_word_boundary(line, abs_pos, abs_pos + m.length(0), len)) {
          matched = true;
          break;
        }
        search_start = m.suffix().first;
        if (search_start == s.cend()) { break;
}
      }
    } else {
      matched = std::regex_search(line, *re);
    }

    if (opts->invert_match != 0) {
      matched = !matched;
    }

    if (matched) {
      match_count++;
      if (opts->max_count > 0 && match_count >= opts->max_count) {
        // Stop after max_count matches for this file
        if (opts->count_only != 0) {
          continue;
        }
        // Still need to print the match that reached the limit
        if (opts->quiet == 0) {
          if (opts->files_with_matches != 0) {
            // -l: just print filename once per file
            // NOLINTNEXTLINE(bugprone-narrowing-conversions)
            printf("%s\n", (display_name != nullptr) ? display_name : "(standard input)");
            match_count = 1;
            goto done;
          }
          search_print_match(line, len, opts->line_number, line_count,
                              (use_prefix != 0) ? display_name : nullptr, use_color, re,
                              opts->pattern, opts->only_matching,
                              static_cast<int>(opts->mode == GrepMode::FIXED));
        }
        goto done;
      }
      if (opts->count_only != 0) {
        continue;
      }
      if (opts->quiet != 0) {
        continue;
      }
      if (opts->files_with_matches != 0) {
        // -l: just print filename once per file
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        printf("%s\n", (display_name != nullptr) ? display_name : "(standard input)");
        match_count = 1; // signal that we found a match
        goto done;
      }
      search_print_match(line, len, opts->line_number, line_count,
                          (use_prefix != 0) ? display_name : nullptr, use_color, re,
                          opts->pattern, opts->only_matching,
                          static_cast<int>(opts->mode == GrepMode::FIXED));
    }
  }

done:
  if ((opts->count_only != 0) && (opts->files_with_matches == 0)) {
    if (display_name != nullptr) {
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
static int search_directory(const char* dirpath, const GrepOptions* opts,
                             const std::regex* re) {
  std::error_code ec;
  std::filesystem::path const dir(dirpath);

  int total_matches = 0;

  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      if (opts->no_messages == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "grep: %s: %s\n", dirpath, strerror(errno));
      }
      return total_matches;
    }

    std::string const filename = entry.path().filename().string();
    if (filename == "." || filename == "..") {
      continue;
    }

    std::string const full_path = entry.path().string();
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        // Only descend if recursive
        total_matches += search_directory(full_path.c_str(), opts, re);
      } else if (S_ISREG(st.st_mode)) {
        int const matches =
            search_file(full_path.c_str(), false, full_path.c_str(), opts, re);
        if (matches > 0) {
          total_matches += matches;
        }
      }
    }
  }

  return total_matches;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int grep_command(int argc, char** argv) {
  GrepOptions opts;
  opts.mode = GrepMode::BASIC;
  opts.color_mode = GrepColor::NEVER;

  // Handle --color= without argtable (same pattern as ls --color)
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--color") == 0) {
      argv[i] = "--color=always";
      break;
    }
  }

  // Handle --tui without argtable (bypasses arg_lit0 long-only issues)
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--tui") == 0) {
      for (int j = i; j < argc - 1; j++) {
        argv[j] = argv[j + 1];
      }
      argc--;
      grep_tui_main(argc, argv);
      return 0;
    }
  }

  struct arg_lit* extended_opt =
      arg_lit0("E", "extended-regexp", "interpret pattern as extended regex (ERE)");
  struct arg_lit* fixed_opt =
      arg_lit0("F", "fixed-strings", "interpret pattern as fixed strings");
  struct arg_lit* ignore_case_opt =
      arg_lit0("i", "ignore-case", "ignore case distinctions");
  struct arg_lit* invert_opt =
      arg_lit0("v", "invert-match", "select non-matching lines");
  struct arg_lit* line_number_opt =
      arg_lit0("n", "line-number", "print line number with output lines");
  struct arg_lit* count_opt =
      arg_lit0("c", "count", "print only a count of matching lines per file");
  struct arg_lit* recursive_opt =
      arg_lit0("r", "recursive", "read all files under directories recursively");
  struct arg_lit* recursive2_opt =
      arg_lit0("R", "dereference-recursive",
               "read all files under directories recursively (follow symlinks)");
  struct arg_lit* word_regexp_opt =
      arg_lit0("w", "word-regexp", "match only whole words");
  struct arg_lit* line_regexp_opt =
      arg_lit0("x", "line-regexp", "match only whole lines");
  struct arg_lit* only_matching_opt =
      arg_lit0("o", "only-matching", "show only matched part of line");
  struct arg_lit* files_opt =
      arg_lit0("l", "files-with-matches",
               "print only names of FILEs with selected lines");
  struct arg_lit* with_filename_opt =
      arg_lit0("H", "with-filename", "print file name with output lines");
  struct arg_lit* no_filename_opt =
      arg_lit0("h", "no-filename", "suppress file name prefix on output");
  struct arg_str* color_opt =
      arg_str0(nullptr, "color", "WHEN",
               "use markers to highlight matched strings; "
               "WHEN can be 'always', 'auto', or 'never'");
  struct arg_str* pattern_opt =
      arg_str0("e", "regexp", "PATTERN",
               "use PATTERN as the pattern (protect patterns starting with -)");
  struct arg_lit* help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_int* after_ctx_opt =
      arg_int0("A", "after-context", "NUM", "print NUM lines after each match");
  struct arg_int* before_ctx_opt =
      arg_int0("B", "before-context", "NUM", "print NUM lines before each match");
  struct arg_int* context_opt =
      arg_int0("C", "context", "NUM", "print NUM lines before and after each match");
  struct arg_int* max_count_opt =
      arg_int0("m", "max-count", "NUM", "stop after NUM selected lines");
  struct arg_lit* quiet_opt =
      arg_lit0("q", "quiet", "suppress normal output");
  struct arg_lit* no_messages_opt =
      arg_lit0("s", "no-messages", "suppress error messages");
  struct arg_lit* byte_offset_opt =
      arg_lit0("b", "byte-offset", "print byte offset of each match");
  struct arg_lit* null_output_opt =
      arg_lit0("Z", "null", "separate results with NUL instead of newline");
  struct arg_lit* null_data_opt =
      arg_lit0("z", "null-data", "treat input and output as NUL-terminated records");
  struct arg_str* label_opt =
      arg_str0(nullptr, "label", "LABEL", "use LABEL instead of (standard input)");
  struct arg_str* include_opt =
      arg_strn(nullptr, "include", "GLOB", 0, 0, "search only files matching GLOB");
  struct arg_str* exclude_opt =
      arg_strn(nullptr, "exclude", "GLOB", 0, 0, "skip files matching GLOB");
  struct arg_str* directories_opt =
      arg_str0(nullptr, "directories", "ACTION", "how to handle directories (read|skip|recurse)");
  struct arg_str* group_sep_opt =
      arg_str0(nullptr, "group-separator", "SEP", "use SEP as group separator");
  struct arg_lit* no_group_sep_opt =
      arg_lit0(nullptr, "no-group-separator", "do not print group separators");
  struct arg_lit* perl_regexp_opt =
      arg_lit0("P", "perl-regexp", "use Perl-compatible regex (not supported)");
  struct arg_file* file_arg =
      arg_filen(nullptr, nullptr, "FILE", 0, GREP_MAX_FILES, "file to search");
  struct arg_end* end = arg_end(36);

    ArgTable at({extended_opt, fixed_opt,
                  ignore_case_opt, invert_opt, line_number_opt,
                  count_opt, recursive_opt, recursive2_opt,
                  word_regexp_opt, line_regexp_opt, only_matching_opt,
                  files_opt, with_filename_opt, no_filename_opt,
                  color_opt, pattern_opt, help_opt,
                  after_ctx_opt, before_ctx_opt, context_opt,
                  max_count_opt, quiet_opt, no_messages_opt,
                  byte_offset_opt, null_output_opt, null_data_opt,
                  label_opt, include_opt, exclude_opt,
                  directories_opt, group_sep_opt, no_group_sep_opt,
                  perl_regexp_opt,
                  file_arg, end});
    int const nerrors = at.parse(argc, argv);

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
    printf("  -b, --byte-offset         print byte offset of each match\n");
    printf("  -Z, --null                separate results with NUL\n");
    printf("  -q, --quiet               suppress normal output\n");
    printf("  -s, --no-messages         suppress error messages\n");
    printf("  -m, --max-count=NUM       stop after NUM selected lines\n");
    printf("      --color=WHEN          highlight matching text; WHEN can be always, auto, never\n");
    printf("      --label=LABEL         use LABEL for stdin\n");
    printf("      --tui                 interactive TUI viewer\n");
    printf("\n");
    printf("Context control:\n");
    printf("  -A, --after-context=NUM   print NUM lines after each match\n");
    printf("  -B, --before-context=NUM  print NUM lines before each match\n");
    printf("  -C, --context=NUM         print NUM lines before and after each match\n");
    printf("      --group-separator=SEP use SEP as group separator\n");
    printf("      --no-group-separator  disable group separators\n");
    printf("\n");
    printf("File selection:\n");
    printf("  -r, --recursive           search directories recursively\n");
    printf("  -R                        like -r, but follow all symlinks\n");
    printf("  -d, --directories=ACTION  how to handle directories (read|skip|recurse)\n");
    printf("      --include=GLOB        search only files matching GLOB\n");
    printf("      --exclude=GLOB        skip files matching GLOB\n");
    printf("  -z, --null-data           treat input and output as NUL-terminated records\n");
    printf("\n");
    printf("Exit status:\n");
    printf("  0  if a match is found\n");
    printf("  1  if no match was found\n");
    printf("  2  if an error occurred\n");
    printf("\n");
    printf("Note: -P/--perl-regexp is not supported. Basic regex uses std::regex syntax (ECMAScript).\n");
    
    return 0;
  }

if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

  // --- Parse options ---
  if (extended_opt->count > 0) {
    opts.mode = GrepMode::EXTENDED;
  }
  if (fixed_opt->count > 0) {
    opts.mode = GrepMode::FIXED;
  }

  opts.ignore_case = static_cast<int>(ignore_case_opt->count > 0);
  opts.invert_match = static_cast<int>(invert_opt->count > 0);
  opts.line_number = static_cast<int>(line_number_opt->count > 0);
  opts.count_only = static_cast<int>(count_opt->count > 0);
  opts.recursive = static_cast<int>((recursive_opt->count > 0) || (recursive2_opt->count > 0));
  opts.word_regexp = static_cast<int>(word_regexp_opt->count > 0);
  opts.line_regexp = static_cast<int>(line_regexp_opt->count > 0);
  opts.only_matching = static_cast<int>(only_matching_opt->count > 0);
  opts.files_with_matches = static_cast<int>(files_opt->count > 0);
  opts.always_show_filename = static_cast<int>(with_filename_opt->count > 0);
  opts.never_show_filename = static_cast<int>(no_filename_opt->count > 0);

  if (after_ctx_opt->count > 0) opts.after_context = static_cast<int>(after_ctx_opt->ival[0]);
  if (before_ctx_opt->count > 0) opts.before_context = static_cast<int>(before_ctx_opt->ival[0]);
  if (context_opt->count > 0) {
    opts.context = static_cast<int>(context_opt->ival[0]);
    opts.after_context = opts.context;
    opts.before_context = opts.context;
  }
  if (max_count_opt->count > 0) opts.max_count = static_cast<int>(max_count_opt->ival[0]);
  opts.quiet = static_cast<int>(quiet_opt->count > 0);
  opts.no_messages = static_cast<int>(no_messages_opt->count > 0);
  opts.byte_offset = static_cast<int>(byte_offset_opt->count > 0);
  opts.null_output = static_cast<int>(null_output_opt->count > 0);
  opts.null_data = static_cast<int>(null_data_opt->count > 0);
  if (label_opt->count > 0) opts.label = label_opt->sval[0];
  for (int i = 0; i < include_opt->count; ++i) opts.include_globs.push_back(include_opt->sval[i]);
  for (int i = 0; i < exclude_opt->count; ++i) opts.exclude_globs.push_back(exclude_opt->sval[i]);
  if (directories_opt->count > 0) {
    const char* v = directories_opt->sval[0];
    if (strcmp(v, "skip") == 0) opts.directories_action = 1;
    else if (strcmp(v, "recurse") == 0) opts.directories_action = 2;
    else if (strcmp(v, "read") == 0) opts.directories_action = 0;
    else {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "grep: invalid argument '%s' for --directories\n", v);
    }
  }
  if (group_sep_opt->count > 0) opts.group_separator = group_sep_opt->sval[0];
  opts.no_group_separator = static_cast<int>(no_group_sep_opt->count > 0);
  if (perl_regexp_opt->count > 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "grep: support for -P is not compiled in\n");
    exit(2);
  }

  if (color_opt->count > 0) {
    const char* val = color_opt->sval[0];
    if (strcmp(val, "always") == 0) {
      opts.color_mode = GrepColor::ALWAYS;
    } else if (strcmp(val, "auto") == 0) {
      opts.color_mode = GrepColor::AUTO;
    } else if (strcmp(val, "never") == 0) {
      opts.color_mode = GrepColor::NEVER;
    } else {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr,
                    "grep: invalid argument '%s' for --color\n"
                    "Valid arguments: always, auto, never\n",
                    val);
    }
  }

  // --- Get pattern ---
  const char* pattern = nullptr;
  if (pattern_opt->count > 0) {
    pattern = pattern_opt->sval[0];
  }

  // If no -e, pattern is the first positional argument
  if (pattern == nullptr && file_arg->count > 0) {
    pattern = file_arg->filename[0];
    // Shift remaining filenames down
    file_arg->count--;
    for (int i = 0; i < file_arg->count; i++) {
      file_arg->filename[i] = file_arg->filename[i + 1];
    }
  }

  if (pattern == nullptr) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "grep: no pattern specified\n");
    
    exit(2); // NOLINT(misc-include-cleaner)
  }

  opts.pattern = pattern;

  // --- Compile regex (if not fixed mode) ---
  std::optional<std::regex> re_opt;
  const std::regex* re = nullptr;
  if (opts.mode != GrepMode::FIXED) {
    auto re_flags = std::regex::optimize;
    if (opts.ignore_case != 0) {
      re_flags |= std::regex::icase;
    }
    try {
      re_opt.emplace(search_compile_pattern(opts.pattern, opts.word_regexp,
                                             opts.line_regexp, re_flags));
      re = &*re_opt;
    } catch (const std::regex_error& e) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "grep: invalid pattern '%s': %s\n", pattern,
                    e.what());
      
      exit(2); // NOLINT(misc-include-cleaner)
    }
  }

  // --- Process files ---
  int total_matches = 0;
  bool error_occurred = false;
  bool const has_files = (file_arg->count > 0);

  if (!has_files || (file_arg->count == 1 &&
                     strcmp(file_arg->filename[0], "-") == 0)) {
    // stdin mode
    total_matches +=
        search_file(nullptr, true, nullptr, &opts, re);
  } else if ((opts.recursive != 0) && has_files) {
    // Recursive mode: treat arguments as directories/files
    for (int i = 0; i < file_arg->count; i++) {
      struct stat st;
      if (stat(file_arg->filename[i], &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
          total_matches += search_directory(
              file_arg->filename[i], &opts, re);
        } else {
          total_matches += search_file(
              file_arg->filename[i], false, file_arg->filename[i],
              &opts, re);
        }
      } else {
        error_occurred = true;
        if (opts.no_messages == 0) {
          // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
          (void)fprintf(stderr, "grep: %s: %s\n", file_arg->filename[i],
                        strerror(errno));
        }
      }
    }
  } else {
    // Normal mode: search files
    int const force_prefix = static_cast<int>((file_arg->count > 1) || (opts.always_show_filename) != 0);
    for (int i = 0; i < file_arg->count; i++) {
      const char* fname = file_arg->filename[i];
      struct stat st;
      if (stat(fname, &st) == 0 && S_ISDIR(st.st_mode)) {
        error_occurred = true;
        if (opts.no_messages == 0) {
          // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
          (void)fprintf(stderr,
                        "grep: %s: Is a directory (use -r for recursive)\n",
                        fname);
        }
        continue;
      }
      if (stat(fname, &st) != 0) {
        error_occurred = true;
        if (opts.no_messages == 0) {
          // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
          (void)fprintf(stderr, "grep: %s: %s\n", fname, strerror(errno));
        }
        continue;
      }
      int const matches =
          search_file(fname, false,
                      (force_prefix != 0) ? fname : nullptr, &opts, re);
      total_matches += matches;
    }
  }

  // --- Cleanup ---
  // regex destructor handles cleanup
  

  // Exit with 0 if match found, 1 otherwise, 2 on error
  if (error_occurred && !(opts.quiet != 0 && total_matches > 0)) {
    exit(2); // NOLINT(misc-include-cleaner)
  }
  if (total_matches > 0) {
    exit(0); // NOLINT(misc-include-cleaner)
  }
  exit(1); // NOLINT(misc-include-cleaner, misc-unreachable-code)
}

REGISTER_COMMAND("grep", grep_command, "Search for patterns in files");
