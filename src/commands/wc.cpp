#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "commands/wc.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/json_stringifier.hpp"

struct WcCounts {
    int64_t lines = 0;
    int64_t words = 0;
    int64_t bytes = 0;
    int64_t chars = 0;
};

static void print_counts(const WcCounts& counts, const char* name, bool show_l, bool show_w,
                          bool show_c, bool show_m) {
    if (show_l) { printf(" %7lld", static_cast<long long>(counts.lines)); }
    if (show_w) { printf(" %7lld", static_cast<long long>(counts.words)); }
    if (show_c || show_m) {
        if (show_m) {
            printf(" %7lld", static_cast<long long>(counts.chars));
        } else {
            printf(" %7lld", static_cast<long long>(counts.bytes));
        }
    }
    if (name != nullptr) { printf(" %s", name); }
    printf("\n");
}

static WcCounts wc_stream(FILE* fp, const char* name, bool show_l, bool show_w,
                          bool show_c, bool show_m, WcCounts* total, bool json_mode) {
    WcCounts counts;
    bool in_word = false;
    // For -m (character count), decode UTF-8 properly so multibyte sequences
    // count as one character; for -c (byte count) the raw byte counter is used.
    mbstate_t st = mbstate_t{};
    int c;
    while ((c = fgetc(fp)) != EOF) {
        counts.bytes++;
        if (show_m) {
            char const mb[1] = { static_cast<char>(c) };
            if (mbrlen(mb, 1, &st) > 0) {
                counts.chars++;
            }
        } else {
            counts.chars++;
        }
        if (c == '\n') { counts.lines++; }
        bool const is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r'
                         || c == '\v' || c == '\f');
        if (is_space) {
            if (in_word) {
                counts.words++;
                in_word = false;
            }
        } else {
            in_word = true;
        }
    }
    if (in_word) { counts.words++; }

    if (!json_mode) {
        print_counts(counts, name, show_l, show_w, show_c, show_m);
    }

    if (total != nullptr) {
        total->lines += counts.lines;
        total->words += counts.words;
        total->bytes += counts.bytes;
        total->chars += counts.chars;
    }

    return counts;
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTION]... [FILE]...\n", prog);
    printf("Print newline, word, and byte counts for each FILE.\n");
    printf("\n");
    printf("  -c, --bytes      print the byte counts\n");
    printf("  -m, --chars      print the character counts\n");
    printf("  -l, --lines      print the newline counts\n");
    printf("  -w, --words      print the word counts\n");
    printf("      --json       output in JSON format\n");
    printf("  -h, --help       display this help and exit\n");
    printf("\n");
    printf("With no FILE, read standard input.\n");
}

int wc_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* lines_opt = arg_lit0("l", "lines", "print the newline counts");
    struct arg_lit* words_opt = arg_lit0("w", "words", "print the word counts");
    struct arg_lit* bytes_opt = arg_lit0("c", "bytes", "print the byte counts");
    struct arg_lit* chars_opt = arg_lit0("m", "chars", "print the character counts");
    struct arg_lit* json_opt = arg_lit0(NULL, "json", "output in JSON format");
    struct arg_file* files_opt = arg_filen(NULL, NULL, "FILE", 0, 1000, "files to count");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, lines_opt, words_opt, bytes_opt, chars_opt, json_opt, files_opt, end});

    int const nerrors = at.parse(argc, argv);
    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    bool show_l = lines_opt->count > 0;
    bool show_w = words_opt->count > 0;
    bool show_c = bytes_opt->count > 0;
    bool show_m = chars_opt->count > 0;
    bool json_mode = json_opt->count > 0;

    // GNU default: when no -c/-m/-l/-w is given, show lines, words, bytes
    if (!show_l && !show_w && !show_c && !show_m) {
        show_l = show_w = show_c = true;
    }

    struct WcResult {
        bool valid = true;
        std::string error;
        WcCounts counts;
        std::string name;
    };

    std::vector<WcResult> results;
    WcCounts totals;
    int success_count = 0;

    if (files_opt->count == 0) {
        // Read from stdin
        WcCounts const c = wc_stream(stdin, nullptr, show_l, show_w, show_c, show_m, nullptr, json_mode);
        if (json_mode) {
            WcResult r;
            r.counts = c;
            r.name = "";
            results.push_back(r);
        }
    } else {
        for (int i = 0; i < files_opt->count; i++) {
            const char* fname = files_opt->filename[i];
            if (strcmp(fname, "-") == 0) {
                WcCounts const c = wc_stream(stdin, "-", show_l, show_w, show_c, show_m, &totals, json_mode);
                if (json_mode) {
                    WcResult r;
                    r.counts = c;
                    r.name = "-";
                    results.push_back(r);
                }
                success_count++;
            } else {
                FILE* fp = fopen(fname, "r");
                if (fp == nullptr) {
                    if (json_mode) {
                        WcResult r;
                        r.valid = false;
                        r.error = strerror(errno);
                        r.name = fname;
                        results.push_back(r);
                    } else {
                        (void)fprintf(stderr, "wc: %s: %s\n", fname, strerror(errno));
                    }
                } else {
                    WcCounts const c = wc_stream(fp, fname, show_l, show_w, show_c, show_m, &totals, json_mode);
                    if (json_mode) {
                        WcResult r;
                        r.counts = c;
                        r.name = fname;
                        results.push_back(r);
                    }
                    (void)fclose(fp);
                    success_count++;
                }
            }
        }
    }

    // Exit code logic: 0 if all files processed successfully, 1 if any file failed
    // (GNU: "Exit status is 0 if no errors occurred, otherwise 1.")
    int const any_failure = (success_count != files_opt->count && files_opt->count > 0);

    if (json_mode) {
        (void)fprintf(stdout, "[\n");
        for (size_t i = 0; i < results.size(); i++) {
            const WcResult& r = results[i];
            if (!r.valid) {
                (void)fprintf(stdout, "  {\n");
                (void)fprintf(stdout, "    \"error\": ");
                json_escape_string(stdout, r.error.c_str());
                (void)fprintf(stdout, ",\n");
                (void)fprintf(stdout, "    \"name\": ");
                json_escape_string(stdout, r.name.c_str());
                (void)fprintf(stdout, "\n");
                (void)fprintf(stdout, "  }%s\n", (i + 1 < results.size()) ? "," : "");
            } else {
                const WcCounts& c = r.counts;
                (void)fprintf(stdout, "  {\n");
                (void)fprintf(stdout, "    \"bytes\": %lld,\n", static_cast<long long>(c.bytes));
                (void)fprintf(stdout, "    \"chars\": %lld,\n", static_cast<long long>(c.chars));
                (void)fprintf(stdout, "    \"lines\": %lld,\n", static_cast<long long>(c.lines));
                (void)fprintf(stdout, "    \"name\": ");
                json_escape_string(stdout, r.name.c_str());
                (void)fprintf(stdout, ",\n");
                (void)fprintf(stdout, "    \"words\": %lld\n", static_cast<long long>(c.words));
                (void)fprintf(stdout, "  }%s\n", (i + 1 < results.size()) ? "," : "");
            }
        }
        // Total entry (matching test expectation: 3 entries for 2 files, where 3rd is total)
        if (success_count > 1) {
            (void)fprintf(stdout, "  ,\n");
            (void)fprintf(stdout, "  {\n");
            (void)fprintf(stdout, "    \"bytes\": %lld,\n", static_cast<long long>(totals.bytes));
            (void)fprintf(stdout, "    \"chars\": %lld,\n", static_cast<long long>(totals.chars));
            (void)fprintf(stdout, "    \"lines\": %lld,\n", static_cast<long long>(totals.lines));
            (void)fprintf(stdout, "    \"name\": \"total\",\n");
            (void)fprintf(stdout, "    \"words\": %lld\n", static_cast<long long>(totals.words));
            (void)fprintf(stdout, "  }\n");
        }
        (void)fprintf(stdout, "]\n");
        return any_failure ? 1 : 0;
    }

    // Non-JSON: print total if more than one file was successfully processed
    if (success_count > 1) {
        print_counts(totals, "total", show_l, show_w, show_c, show_m);
    }
    return any_failure ? 1 : 0;
}

REGISTER_COMMAND("wc", wc_command, "Print byte, word, and line counts");