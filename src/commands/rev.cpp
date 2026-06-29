#include <cstdio>
#include <cstring>
#include <argtable3.h>
#include "commands/rev.hpp"

static void reverse_string(char* s, size_t len) {
    if (len == 0) return;
    size_t start = 0;
    size_t end = len - 1;
    while (start < end) {
        char tmp = s[start];
        s[start] = s[end];
        s[end] = tmp;
        start++;
        end--;
    }
}

static void rev_file(FILE* fp) {
    char buf[8192];
    while (fgets(buf, (int)sizeof(buf), fp)) {
        size_t len = strlen(buf);
        int had_newline = (len > 0 && buf[len - 1] == '\n');
        if (had_newline) {
            buf[len - 1] = '\0';
            len--;
        }
        reverse_string(buf, len);
        fprintf(stdout, "%s\n", buf);
    }
}

void rev_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Reverse lines characterwise.\n");
        printf("\n");
        printf("  -h, --help    display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (file_arg->count == 0) {
        rev_file(stdin);
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            const char* fname = file_arg->filename[i];
            if (strcmp(fname, "-") == 0) {
                rev_file(stdin);
            } else {
                FILE* fp = fopen(fname, "r");
                if (fp == NULL) {
                    fprintf(stderr, "rev: %s: No such file or directory\n", fname);
                    continue;
                }
                rev_file(fp);
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
