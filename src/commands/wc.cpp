#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands/wc.hpp"

static void print_counts(int64_t lines, int64_t words, int64_t bytes,
                         const char* name, bool show_l, bool show_w,
                         bool show_c, bool show_m) {
    if (show_l) printf(" %7lld", (long long)lines);
    if (show_w) printf(" %7lld", (long long)words);
    if (show_c || show_m) {
        if (show_m)
            printf(" %7lld", (long long)bytes);
        else
            printf(" %7lld", (long long)bytes);
    }
    if (name != NULL) printf(" %s", name);
    printf("\n");
}

static void wc_stream(FILE* fp, const char* name, bool show_l, bool show_w,
                      bool show_c, bool show_m, int64_t* total_l,
                      int64_t* total_w, int64_t* total_c) {
    int64_t lines = 0;
    int64_t words = 0;
    int64_t bytes = 0;
    bool in_word = false;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        bytes++;
        if (c == '\n') lines++;
        bool is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r'
                         || c == '\v' || c == '\f');
        if (is_space) {
            if (in_word) {
                words++;
                in_word = false;
            }
        } else {
            in_word = true;
        }
    }
    if (in_word) words++;

    print_counts(lines, words, bytes, name, show_l, show_w, show_c, show_m);
    *total_l += lines;
    *total_w += words;
    *total_c += bytes;
}

void wc_command(int argc, char** argv) {
    bool show_l = false;
    bool show_w = false;
    bool show_c = false;
    bool show_m = false;
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
            printf("  -h, --help       display this help and exit\n");
            printf("\n");
            printf("With no FILE, read standard input.\n");
            return;
        }
        if (strcmp(a, "-c") == 0 || strcmp(a, "--bytes") == 0) {
            show_c = true;
        } else if (strcmp(a, "-m") == 0 || strcmp(a, "--chars") == 0) {
            show_m = true;
        } else if (strcmp(a, "-l") == 0 || strcmp(a, "--lines") == 0) {
            show_l = true;
        } else if (strcmp(a, "-w") == 0 || strcmp(a, "--words") == 0) {
            show_w = true;
        } else if (a[0] == '-' && a[1] != '\0') {
            for (size_t j = 1; a[j] != '\0'; j++) {
                if (a[j] == 'c') show_c = true;
                else if (a[j] == 'm') show_m = true;
                else if (a[j] == 'l') show_l = true;
                else if (a[j] == 'w') show_w = true;
                else {
                    fprintf(stderr, "wc: invalid option -- '%c'\n", a[j]);
                    return;
                }
            }
        } else {
            files.push_back(a);
        }
    }

    if (!show_l && !show_w && !show_c && !show_m) {
        show_l = show_w = show_c = true;
    }

    int64_t total_l = 0, total_w = 0, total_c = 0;
    bool total = false;

    if (files.empty()) {
        wc_stream(stdin, NULL, show_l, show_w, show_c, show_m, &total_l,
                  &total_w, &total_c);
        (void)total;
    } else {
        for (size_t i = 0; i < files.size(); i++) {
            const char* fname = files[i];
            if (strcmp(fname, "-") == 0) {
                wc_stream(stdin, "-", show_l, show_w, show_c, show_m, &total_l,
                          &total_w, &total_c);
            } else {
                FILE* fp = fopen(fname, "r");
                if (fp == NULL) {
                    fprintf(stderr, "wc: %s: No such file or directory\n", fname);
                    continue;
                }
                wc_stream(fp, fname, show_l, show_w, show_c, show_m, &total_l,
                          &total_w, &total_c);
                fclose(fp);
            }
            total = true;
        }
        if (files.size() > 1) {
            print_counts(total_l, total_w, total_c, "total", show_l, show_w,
                         show_c, show_m);
        }
    }
}
