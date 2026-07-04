#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <argtable3.h>

#include "commands/paste.hpp"

#define PASTE_MAX_LINE 1048576

struct FileLines {
    char** lines;
    int count;
};

static FileLines read_file(const char* filename) {
    FileLines result = {NULL, 0};

    FILE* fp = (strcmp(filename, "-") == 0) ? stdin : fopen(filename, "r");
    if (fp == NULL) {
        return result;
    }

    int cap = 1024;
    int n = 0;
    char** lines = (char**)malloc((size_t)cap * sizeof(char*));
    if (!lines) { if (fp != stdin) fclose(fp); return result; }

    char buf[PASTE_MAX_LINE];
    while (fgets(buf, PASTE_MAX_LINE, fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
        if (n >= cap) {
            cap *= 2;
            lines = (char**)realloc(lines, (size_t)cap * sizeof(char*));
            if (!lines) { if (fp != stdin) fclose(fp); return result; }
        }
        lines[n] = strdup(buf);
        if (!lines[n]) { if (fp != stdin) fclose(fp); return result; }
        n++;
    }

    if (fp != stdin) fclose(fp);

    result.lines = lines;
    result.count = n;
    return result;
}

static void free_file_lines(FileLines fl) {
    if (fl.lines) {
        for (int i = 0; i < fl.count; i++) free(fl.lines[i]);
        free(fl.lines);
    }
}

static const char* delim_at(const char* delims, int idx) {
    size_t len = strlen(delims);
    if (len == 0) return "";
    if (len == 1) return delims;
    static char buf[2];
    buf[0] = delims[idx % (int)len];
    buf[1] = '\0';
    return buf;
}

void paste_command(int argc, char** argv) {
    PasteOptions opts;
    opts.delimiters = "\t";
    opts.serial = 0;
    opts.zero_terminated = 0;

    struct arg_str* delimiters_opt = arg_str0("d", "delimiters", "LIST", "reuse characters from LIST instead of TABs");
    struct arg_lit* serial_opt = arg_lit0("s", "serial", "paste one file at a time instead of in parallel");
    struct arg_lit* zero_opt = arg_lit0("z", "zero-terminated", "line delimiter is NUL, not newline");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(nullptr, "version", "output version information and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 1024, "files (or - for stdin)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        delimiters_opt, serial_opt, zero_opt,
        help_opt, version_opt,
        files_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Merge corresponding lines from files.\n");
        printf("\n");
        printf("Write lines consisting of the sequentially corresponding lines from\n");
        printf("each FILE, separated by TABs, to standard output.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -d, --delimiters=LIST   reuse characters from LIST instead of TABs\n");
        printf("  -s, --serial            paste one file at a time instead of in parallel\n");
        printf("  -z, --zero-terminated   line delimiter is NUL, not newline\n");
        printf("      --help              display this help and exit\n");
        printf("      --version           output version information and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (version_opt->count > 0) {
        printf("paste (modbox) 1.0\n");
        printf("Copyright (C) 2026 modbox\n");
        printf("License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>\n");
        printf("This is free software: you are free to change and redistribute it.\n");
        printf("There is NO WARRANTY, to the extent permitted by law.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (delimiters_opt->count > 0) {
        opts.delimiters = delimiters_opt->sval[0];
    }
    opts.serial = (serial_opt->count > 0);
    opts.zero_terminated = (zero_opt->count > 0);

    int nfiles = files_arg->count;

    if (nfiles == 0) {
        nfiles = 1;
    }

    FileLines* files = (FileLines*)malloc((size_t)nfiles * sizeof(FileLines));
    if (!files) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int max_lines = 0;
    int open_error = 0;

    if (files_arg->count == 0) {
        files[0] = read_file("-");
        if (files[0].lines) {
            max_lines = files[0].count;
        }
    } else {
        for (int i = 0; i < nfiles; i++) {
            files[i] = read_file(files_arg->filename[i]);
            if (files[i].lines == NULL && strcmp(files_arg->filename[i], "-") != 0) {
                fprintf(stderr, "paste: %s: %s\n", files_arg->filename[i], strerror(errno));
                open_error = 1;
            }
            if (files[i].count > max_lines) {
                max_lines = files[i].count;
            }
        }
    }

    if (open_error) {
        for (int i = 0; i < nfiles; i++) free_file_lines(files[i]);
        free(files);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    const char line_delim = opts.zero_terminated ? '\0' : '\n';
    int delim_idx = 0;

    if (opts.serial) {
        for (int fi = 0; fi < nfiles; fi++) {
            delim_idx = 0;
            for (int li = 0; li < files[fi].count; li++) {
                if (li > 0) {
                    printf("%s", delim_at(opts.delimiters, delim_idx++));
                }
                printf("%s", files[fi].lines[li]);
            }
            putchar(line_delim);
        }
    } else {
        for (int li = 0; li < max_lines; li++) {
            delim_idx = 0;
            int first = 1;
            for (int fi = 0; fi < nfiles; fi++) {
                if (!first) {
                    printf("%s", delim_at(opts.delimiters, delim_idx++));
                }
                if (li < files[fi].count) {
                    printf("%s", files[fi].lines[li]);
                }
                first = 0;
            }
            putchar(line_delim);
        }
    }

    for (int i = 0; i < nfiles; i++) free_file_lines(files[i]);
    free(files);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
