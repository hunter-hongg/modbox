#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands/wc.hpp"
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
    if (show_l) printf(" %7lld", (long long)counts.lines);
    if (show_w) printf(" %7lld", (long long)counts.words);
    if (show_c || show_m) {
        if (show_m)
            printf(" %7lld", (long long)counts.bytes);
        else
            printf(" %7lld", (long long)counts.bytes);
    }
    if (name != nullptr) printf(" %s", name);
    printf("\n");
}

static WcCounts wc_stream(FILE* fp, const char* name, bool show_l, bool show_w,
                          bool show_c, bool show_m, WcCounts* total, bool json_mode) {
    WcCounts counts;
    bool in_word = false;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        counts.bytes++;
        counts.chars++;
        if (c == '\n') counts.lines++;
        bool is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r'
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
    if (in_word) counts.words++;

    if (!json_mode) {
        print_counts(counts, name, show_l, show_w, show_c, show_m);
    }

    if (total) {
        total->lines += counts.lines;
        total->words += counts.words;
        total->bytes += counts.bytes;
        total->chars += counts.chars;
    }

    return counts;
}

int wc_command(int argc, char** argv) {
    bool show_l = false;
    bool show_w = false;
    bool show_c = false;
    bool show_m = false;
    bool json_mode = false;
    std::vector<const char*> files;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
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
            return 0;
        }
        if (strcmp(a, "-c") == 0 || strcmp(a, "--bytes") == 0) {
            show_c = true;
        } else if (strcmp(a, "-m") == 0 || strcmp(a, "--chars") == 0) {
            show_m = true;
        } else if (strcmp(a, "-l") == 0 || strcmp(a, "--lines") == 0) {
            show_l = true;
        } else if (strcmp(a, "-w") == 0 || strcmp(a, "--words") == 0) {
            show_w = true;
        } else if (strcmp(a, "--json") == 0) {
            json_mode = true;
        } else if (a[0] == '-' && a[1] != '\0') {
            for (size_t j = 1; a[j] != '\0'; j++) {
                if (a[j] == 'c') show_c = true;
                else if (a[j] == 'm') show_m = true;
                else if (a[j] == 'l') show_l = true;
                else if (a[j] == 'w') show_w = true;
                else {
                    fprintf(stderr, "wc: invalid option -- '%c'\n", a[j]);
                    return 0;
                }
            }
        } else {
            files.push_back(a);
        }
    }

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

    if (files.empty()) {
        WcCounts c = wc_stream(stdin, nullptr, show_l, show_w, show_c, show_m, nullptr, json_mode);
        if (json_mode) {
            WcResult r;
            r.counts = c;
            r.name = "";
            results.push_back(r);
        }
    } else {
        for (size_t i = 0; i < files.size(); i++) {
            const char* fname = files[i];
            if (strcmp(fname, "-") == 0) {
                WcCounts c = wc_stream(stdin, "-", show_l, show_w, show_c, show_m, &totals, json_mode);
                if (json_mode) {
                    WcResult r;
                    r.counts = c;
                    r.name = "-";
                    results.push_back(r);
                }
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
                        fprintf(stderr, "wc: %s: No such file or directory\n", fname);
                    }
                } else {
                    WcCounts c = wc_stream(fp, fname, show_l, show_w, show_c, show_m, &totals, json_mode);
                    if (json_mode) {
                        WcResult r;
                        r.counts = c;
                        r.name = fname;
                        results.push_back(r);
                    }
                    fclose(fp);
                    success_count++;
                }
            }
        }
    }

    if (json_mode) {
        fprintf(stdout, "[\n");
        for (size_t i = 0; i < results.size(); i++) {
            const WcResult& r = results[i];
            if (!r.valid) {
                fprintf(stdout, "  {\n");
                fprintf(stdout, "    \"error\": ");
                json_escape_string(stdout, r.error.c_str());
                fprintf(stdout, ",\n");
                fprintf(stdout, "    \"name\": ");
                json_escape_string(stdout, r.name.c_str());
                fprintf(stdout, "\n");
                fprintf(stdout, "  }%s\n", (i + 1 < results.size()) ? "," : "");
            } else {
                const WcCounts& c = r.counts;
                fprintf(stdout, "  {\n");
                fprintf(stdout, "    \"bytes\": %lld,\n", (long long)c.bytes);
                fprintf(stdout, "    \"chars\": %lld,\n", (long long)c.chars);
                fprintf(stdout, "    \"lines\": %lld,\n", (long long)c.lines);
                fprintf(stdout, "    \"name\": ");
                json_escape_string(stdout, r.name.c_str());
                fprintf(stdout, ",\n");
                fprintf(stdout, "    \"words\": %lld\n", (long long)c.words);
                fprintf(stdout, "  }%s\n", (i + 1 < results.size()) ? "," : "");
            }
        }
        if (success_count > 1) {
            fprintf(stdout, "  ,\n");
            fprintf(stdout, "  {\n");
            fprintf(stdout, "    \"bytes\": %lld,\n", (long long)totals.bytes);
            fprintf(stdout, "    \"chars\": %lld,\n", (long long)totals.chars);
            fprintf(stdout, "    \"lines\": %lld,\n", (long long)totals.lines);
            fprintf(stdout, "    \"name\": \"total\",\n");
            fprintf(stdout, "    \"words\": %lld\n", (long long)totals.words);
            fprintf(stdout, "  }\n");
        }
        fprintf(stdout, "]\n");
        return 0;
    }

    if (success_count > 1) {
        print_counts(totals, "total", show_l, show_w, show_c, show_m);
    }
    return 0;
}

REGISTER_COMMAND("wc", wc_command, "Print byte, word, and line counts");
