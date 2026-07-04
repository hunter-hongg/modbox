#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <cctype>
#include <regex>
#include <string>
#include <vector>
#include <argtable3.h>

#include "commands/csplit.hpp"

/* ── Pattern types ────────────────────────────────────────────────────────── */

enum class PatternType {
    LINE,           /* line number */
    REGEX_INCLUDE,  /* /regexp/ — include matching line in current file */
    REGEX_EXCLUDE,  /* %regexp% — exclude matching line (discard it) */
    REPEAT,         /* {N} — repeat previous pattern N more times */
    REPEAT_ALL,     /* {*} — repeat previous pattern infinitely */
};

struct CsplitPattern {
    PatternType type;
    std::string regex_str;  /* for REGEX_INCLUDE / REGEX_EXCLUDE */
    int64_t line_no = 0;    /* for LINE */
    int64_t repeat = 0;     /* for REPEAT */
    int64_t offset = 0;     /* offset after matching line */
    int repeat_forever = 0; /* set by {*} expansion: repeat this pattern forever */
};

/* ── Pattern parsing ──────────────────────────────────────────────────────── */

/* Parse a single pattern from a string.
 * Returns true on success, false on error with message in errmsg. */
static bool parse_pattern(const char *s, CsplitPattern *pat,
                          const CsplitPattern *prev,
                          std::string &errmsg) {
    if (s == NULL || *s == '\0') {
        errmsg = "empty pattern";
        return false;
    }

    /* {N} or {*} — repeat */
    if (s[0] == '{') {
        if (prev == NULL || prev->type == PatternType::REPEAT ||
            prev->type == PatternType::REPEAT_ALL) {
            errmsg = "no previous pattern to repeat";
            return false;
        }
        const char *end = s + 1;
        if (*end == '*') {
            if (*(end + 1) != '}') {
                errmsg = "malformed repeat pattern";
                return false;
            }
            pat->type = PatternType::REPEAT_ALL;
            return true;
        }
        char *ep = NULL;
        // NOLINTNEXTLINE(cert-err34-c)
        long n = strtol(end, &ep, 10);
        if (ep == end || *ep != '}') {
            errmsg = "malformed repeat pattern";
            return false;
        }
        pat->type = PatternType::REPEAT;
        pat->repeat = (int64_t)n;
        return true;
    }

    /* /REGEXP/[OFFSET] or %REGEXP%[OFFSET] */
    if (s[0] == '/' || s[0] == '%') {
        char delim = s[0];
        pat->type = (delim == '/') ? PatternType::REGEX_INCLUDE
                                   : PatternType::REGEX_EXCLUDE;

        /* Find closing delimiter */
        const char *close = NULL;
        const char *p = s + 1;
        int escaped = 0;
        while (*p) {
            if (escaped) {
                escaped = 0;
                p++;
                continue;
            }
            if (*p == '\\') {
                escaped = 1;
                p++;
                continue;
            }
            if (*p == delim) {
                close = p;
                break;
            }
            p++;
        }
        if (close == NULL) {
            errmsg = std::string("unterminated ") + delim + " pattern";
            return false;
        }

        /* Extract regex (between delimiters) */
        pat->regex_str = std::string(s + 1, (size_t)(close - s - 1));

        /* Parse optional offset */
        pat->offset = 0;
        const char *off = close + 1;
        if (*off == '+' || *off == '-' || (*off >= '0' && *off <= '9')) {
            char *ep = NULL;
            // NOLINTNEXTLINE(cert-err34-c)
            pat->offset = (int64_t)strtoll(off, &ep, 10);
            if (ep == off) pat->offset = 0;
        } else if (*off != '\0') {
            errmsg = std::string("garbage after pattern: ") + off;
            return false;
        }
        return true;
    }

    /* LINE_NO (integer, possibly negative) */
    {
        char *ep = NULL;
        // NOLINTNEXTLINE(cert-err34-c)
        long long val = strtoll(s, &ep, 10);
        if (*ep != '\0') {
            errmsg = std::string("invalid pattern: ") + s;
            return false;
        }
        pat->type = PatternType::LINE;
        pat->line_no = (int64_t)val;
        return true;
    }
}

/* Expand patterns: resolve {N} and {*} into the expanded list */
static std::vector<CsplitPattern> expand_patterns(
        const std::vector<CsplitPattern> &raw) {
    std::vector<CsplitPattern> result;
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i].type == PatternType::REPEAT) {
            /* Repeat the previous pattern N times */
            if (result.empty()) continue;
            CsplitPattern base = result.back();
            base.offset = 0; /* offset only applies to first use */
            for (int64_t j = 0; j < raw[i].repeat; j++)
                result.push_back(base);
        } else if (raw[i].type == PatternType::REPEAT_ALL) {
            if (result.empty()) continue;
            /* Mark the last pattern as repeating forever */
            if (!result.empty())
                result.back().repeat_forever = 1;
        } else {
            result.push_back(raw[i]);
        }
    }
    return result;
}

/* ── Core splitting logic ─────────────────────────────────────────────────── */

struct Splitter {
    FILE *in;
    FILE *cur = NULL;
    int64_t cur_file_idx = 0;
    int64_t line_no = 0;       /* current 1-based line number */
    char prefix[256];
    char suffix_fmt[256];
    int quiet;
    int keep_files;
    int elide_empty;
    std::vector<std::string> created_files;
    std::vector<int64_t> file_sizes;
    int has_error = 0;

    Splitter(FILE *in_, const CsplitOptions *opts)
        : in(in_) {
        /* Default prefix "xx", default suffix "%02d" */
        const char *p = opts->prefix ? opts->prefix : "xx";
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(prefix, sizeof(prefix), "%s", p);

        if (opts->suffix_format) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)snprintf(suffix_fmt, sizeof(suffix_fmt), "%s",
                          opts->suffix_format);
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)snprintf(suffix_fmt, sizeof(suffix_fmt), "%%0%dd",
                          opts->digits);
        }

        quiet = opts->quiet;
        keep_files = opts->keep_files;
        elide_empty = opts->elide_empty;
    }

    ~Splitter() {
        if (cur) (void)fclose(cur);
    }

    bool open_next() {
        if (cur) (void)fclose(cur);

        char sfx[128];
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(sfx, sizeof(sfx), suffix_fmt, (long)cur_file_idx);

        std::string fname = prefix;
        fname += sfx;

        cur = fopen(fname.c_str(), "w");
        if (cur == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "csplit: cannot open '%s': %s\n",
                          fname.c_str(), strerror(errno));
            has_error = 1;
            return false;
        }

        created_files.push_back(fname);
        cur_file_idx++;
        return true;
    }

    /* Close current output and record its size. If it's empty and
     * elide_empty is set, remove it. */
    void close_current() {
        if (cur == NULL) return;
        long pos = ftell(cur);
        (void)fclose(cur);
        cur = NULL;

        if (pos == 0 && elide_empty && !created_files.empty()) {
            const std::string &fname = created_files.back();
            (void)remove(fname.c_str());
            created_files.pop_back();
            /* Don't record size for removed files */
            return;
        }
        file_sizes.push_back(pos >= 0 ? (int64_t)pos : 0);
    }

    /* Read a line from input into buf. Returns true if a line was read. */
    bool read_line(std::string &buf) {
        buf.clear();
        int c;
        while ((c = fgetc(in)) != EOF) {
            buf.push_back((char)c);
            if (c == '\n') break;
        }
        if (buf.empty() && feof(in)) return false;
        line_no++;
        return true;
    }

    /* Search forward for a line matching the given regex (optionally offset).
     * Returns true and sets match_line_no if found. */
    bool find_match(const std::string &regex_str, int64_t offset,
                    int64_t &match_line_no) {
        std::regex re;
        try {
            re = std::regex(regex_str);
        } catch (const std::regex_error &) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "csplit: invalid regex: %s\n",
                          regex_str.c_str());
            has_error = 1;
            return false;
        }


        std::vector<std::pair<int64_t, std::string>> matches;
        /* Scan forward looking for matches */
        long start_pos = ftell(in);
        int64_t saved_line = line_no;
        std::string buf;
        while (read_line(buf)) {
            /* Strip trailing newline for regex matching */
            std::string text = buf;
            if (!text.empty() && text.back() == '\n')
                text.pop_back();

            if (std::regex_search(text, re)) {
                matches.push_back({line_no, buf});
            }
        }

        if (matches.empty()) {
            has_error = 1;
            (void)fseek(in, start_pos, SEEK_SET);
            line_no = saved_line;
            return false;
        }

        /* Determine which match to use based on offset */
        /* offset=0: first match; positive N: skip N-1 matches (Nth);
         * negative -N: Nth from last */
        int fail = 0;
        if (offset == 0) {
            match_line_no = matches[0].first;
        } else if (offset > 0) {
            size_t idx = (size_t)(offset - 1);
            if (idx >= matches.size())
                fail = 1;
            else
                match_line_no = matches[idx].first;
        } else {
            size_t idx = matches.size();
            int64_t abs_off = -offset;
            if ((size_t)abs_off > idx)
                fail = 1;
            else
                match_line_no = matches[idx - (size_t)abs_off].first;
        }

        /* Rewind to saved position */
        (void)fseek(in, start_pos, SEEK_SET);
        line_no = saved_line;

        if (fail) {
            has_error = 1;
            return false;
        }
        return true;
    }

    /* Process a single pattern. Returns true if the pattern was matched
     * and a split occurred, false if no match or EOF. */
    bool process_pattern(const CsplitPattern &pat) {
        if (feof(in)) return false;

        if (pat.type == PatternType::LINE) {
            int64_t target = pat.line_no;
            if (target <= 0) {
                has_error = 1;
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr,
                    "csplit: non-positive line numbers not supported\n");
                return false;
            }

            std::string buf;
            while (line_no < target && read_line(buf))
                (void)fputs(buf.c_str(), cur);

            if (line_no >= target && !feof(in)) {
                close_current();
                if (!open_next()) return false;
                return true;
            }
            return false;

        } else if (pat.type == PatternType::REGEX_INCLUDE) {
            int64_t match_line_no = 0;
            if (!find_match(pat.regex_str, pat.offset,
                            match_line_no))
                return false;

            /* Copy lines up to but NOT including the matched line */
            std::string buf;
            while (line_no + 1 < match_line_no && read_line(buf))
                (void)fputs(buf.c_str(), cur);

            /* Output the matching line */
            if (read_line(buf))
                (void)fputs(buf.c_str(), cur);

            close_current();
            if (!open_next()) return false;
            return true;

        } else if (pat.type == PatternType::REGEX_EXCLUDE) {
            int64_t match_line_no = 0;
            if (!find_match(pat.regex_str, pat.offset,
                            match_line_no))
                return false;

            /* Copy lines up to but NOT including the matched line */
            std::string buf;
            while (line_no + 1 < match_line_no && read_line(buf))
                (void)fputs(buf.c_str(), cur);

            /* Discard the matching line */
            if (read_line(buf)) {}

            close_current();
            if (!open_next()) return false;
            return true;
        }

        return false;
    }

    /* Run the split with expanded patterns.
     * Output file sizes are printed to stdout after the run. */
    void run(const std::vector<CsplitPattern> &patterns) {
        if (!open_next()) return;

        /* Check if any pattern has repeat_forever flag */
        int has_repeat_all = 0;
        size_t last_real_idx = 0;
        for (size_t i = 0; i < patterns.size(); i++) {
            if (patterns[i].repeat_forever) {
                has_repeat_all = 1;
                last_real_idx = i;
            }
        }

        if (has_repeat_all) {
            /* Process patterns up to the REPEAT_ALL one */
            for (size_t i = 0; i < last_real_idx; i++) {
                if (feof(in)) break;
                if (!process_pattern(patterns[i])) break;
            }

            /* Now repeat the last pattern indefinitely */
            if (!feof(in)) {
                while (process_pattern(patterns[last_real_idx])) {
                    /* keep going until no more matches */
                }
            }
        } else {
            /* No REPEAT_ALL — process all patterns once */
            for (size_t i = 0; i < patterns.size(); i++) {
                if (feof(in)) break;
                if (!process_pattern(patterns[i])) break;
            }
        }

        /* Write remaining input to current file */
        if (cur) {
            std::string buf;
            while (read_line(buf))
                (void)fputs(buf.c_str(), cur);
            close_current();
        }

        /* Print sizes to stdout */
        if (!quiet) {
            for (size_t i = 0; i < file_sizes.size(); i++)
                printf("%ld\n", (long)file_sizes[i]);
        }
    }
};

/* ── Main command ─────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void csplit_command(int argc, char **argv) {
    CsplitOptions opts = {0};

    struct arg_str *prefix_opt = arg_str0("f", "prefix", "PREFIX",
                                          "output file prefix (default \"xx\")");
    struct arg_str *suffix_fmt_opt = arg_str0("b", "suffix-format", "FORMAT",
                                              "sprintf format for suffix (default \"%%02d\")");
    struct arg_int *digits_opt = arg_int0("n", "digits", "DIGITS",
                                          "number of digits in suffix (default 2)");
    struct arg_lit *elide_opt = arg_lit0("z", "elide-empty-files",
                                         "remove empty output files");
    struct arg_lit *quiet_opt = arg_lit0("s", "quiet",
                                         "do not print output file sizes");
    struct arg_lit *silent_opt = arg_lit0(NULL, "silent",
                                          "alias for --quiet");
    struct arg_lit *keep_opt = arg_lit0("k", "keep-files",
                                        "do not remove output files on error");
    struct arg_lit *help_opt = arg_lit0("h", "help",
                                        "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 1000,
                                          "input file and patterns");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        prefix_opt, suffix_fmt_opt, digits_opt,
        elide_opt, quiet_opt, silent_opt, keep_opt,
        help_opt, file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... FILE PATTERN...\n", argv[0]);
        printf("Split a file into sections determined by context lines.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -f, --prefix=PREFIX     use PREFIX for output files (default \"xx\")\n");
        printf("  -b, --suffix-format=FORMAT  use sprintf FORMAT (default \"%%02d\")\n");
        printf("  -n, --digits=DIGITS     number of digits in suffix (default 2)\n");
        printf("  -z, --elide-empty-files remove empty output files\n");
        printf("  -s, --quiet, --silent   do not print output file sizes\n");
        printf("  -k, --keep-files        do not remove output files on error\n");
        printf("  -h, --help              display this help and exit\n");
        printf("\n");
        printf("PATTERNs:\n");
        printf("  LINE_NO          split at line number LINE_NO\n");
        printf("  /REGEXP/[OFFSET] split at line matching REGEXP (include)\n");
        printf("  %%REGEXP%%[OFFSET] split at line matching REGEXP (exclude)\n");
        printf("  {N}              repeat previous pattern N more times\n");
        printf("  {*}              repeat previous pattern as many times as possible\n");
        printf("\n");
        printf("Output files are named PREFIX00, PREFIX01, ... (or per -b format).\n");
        printf("File sizes are printed to stdout.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.prefix = (prefix_opt->count > 0) ? prefix_opt->sval[0] : NULL;
    opts.suffix_format = (suffix_fmt_opt->count > 0) ? suffix_fmt_opt->sval[0] : NULL;
    opts.elide_empty = (elide_opt->count > 0);
    opts.quiet = (quiet_opt->count > 0) || (silent_opt->count > 0);
    opts.keep_files = (keep_opt->count > 0);

    if (digits_opt->count > 0) {
        opts.digits = digits_opt->ival[0];
        if (opts.digits < 1) opts.digits = 1;
    }

    /* Open input file */
    FILE *in = stdin;
    int opened = 0;

    if (file_arg->count > 0) {
        const char *input_name = file_arg->filename[0];
        if (strcmp(input_name, "-") != 0) {
            in = fopen(input_name, "r");
            if (in == NULL) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "csplit: %s: %s\n", input_name,
                              strerror(errno));
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
            opened = 1;
        }
    }

    /* Collect patterns from remaining positional args (after FILE).
     * file_arg->filename[0] is FILE, [1..count-1] are patterns. */
    std::vector<CsplitPattern> raw_patterns;
    std::string errmsg;

    for (int i = 1; i < file_arg->count; i++) {
        const char *pat_str = file_arg->filename[i];
        CsplitPattern pat;
        CsplitPattern *prev = raw_patterns.empty() ? NULL
            : &raw_patterns.back();

        if (!parse_pattern(pat_str, &pat, prev, errmsg)) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "csplit: %s: %s\n", pat_str, errmsg.c_str());
            if (opened) (void)fclose(in);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        raw_patterns.push_back(pat);
    }

    /* Expand patterns */
    std::vector<CsplitPattern> patterns = expand_patterns(raw_patterns);

    /* Run splitter */
    Splitter splitter(in, &opts);
    splitter.run(patterns);

    if (opened) (void)fclose(in);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
