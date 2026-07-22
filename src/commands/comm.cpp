#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <argtable3.h>

#include "commands/comm.hpp"
#include "commands/command_macros.hpp"

/* ── Constants ──────────────────────────────────────────────────────────── */

#define COMM_MAX_LINE 1048576  /* 1 MiB max line length */

/* ── Line comparison ────────────────────────────────────────────────────── */

/* Returns -1 if a < b, 0 if a == b, 1 if a > b */
static int compare_lines(const char* a, const char* b, const CommOptions* opts) {
    if (opts->ignore_case) {
        while (*a && *b) {
            int ca = std::tolower((unsigned char)*a);
            int cb = std::tolower((unsigned char)*b);
            if (ca != cb) return (ca < cb) ? -1 : 1;
            a++; b++;
        }
        if (*a == *b) return 0;
        return (*a == '\0') ? -1 : 1;
    }
    return strcmp(a, b);
}

/* ── Read file into lines ──────────────────────────────────────────────── */

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

    char buf[COMM_MAX_LINE];
    while (fgets(buf, COMM_MAX_LINE, fp)) {
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

/* ── Check if lines are sorted ──────────────────────────────────────────── */

static int check_sorted(const char** lines, int count, const CommOptions* opts) {
    for (int i = 1; i < count; i++) {
        if (compare_lines(lines[i - 1], lines[i], opts) > 0) {
            return 0; /* not sorted */
        }
    }
    return 1; /* sorted */
}

/* ── Main command ───────────────────────────────────────────────────────── */

void comm_command(int argc, char** argv) {
    CommOptions opts = {0};
    opts.check_order = 1;
    opts.output_delimiter = "\t";

    struct arg_lit* suppress_1_opt = arg_lit0("1", NULL, "suppress column 1 (lines unique to FILE1)");
    struct arg_lit* suppress_2_opt = arg_lit0("2", NULL, "suppress column 2 (lines unique to FILE2)");
    struct arg_lit* suppress_3_opt = arg_lit0("3", NULL, "suppress column 3 (lines that appear in both)");
    struct arg_lit* ignore_case_opt = arg_lit0("i", "ignore-case", "ignore case when comparing");
    struct arg_lit* check_order_opt = arg_lit0(nullptr, "check-order", "check that the input is correctly sorted");
    struct arg_lit* nocheck_order_opt = arg_lit0(nullptr, "nocheck-order", "do not check that the input is correctly sorted");
    struct arg_str* output_delim_opt = arg_str0(nullptr, "output-delimiter", "STR", "separate columns with STR");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file1_arg = arg_filen(NULL, NULL, "FILE1", 1, 1, "first file (or - for stdin)");
    struct arg_file* file2_arg = arg_filen(NULL, NULL, "FILE2", 1, 1, "second file (or - for stdin)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        suppress_1_opt, suppress_2_opt, suppress_3_opt,
        ignore_case_opt, check_order_opt, nocheck_order_opt,
        output_delim_opt,
        help_opt,
        file1_arg, file2_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... FILE1 FILE2\n", argv[0]);
        printf("Compare two sorted files line by line.\n");
        printf("\n");
        printf("With no options, produce three-column output. Column one contains\n");
        printf("lines unique to FILE1, column two contains lines unique to FILE2,\n");
        printf("and column three contains lines common to both files.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -1                      suppress column 1 (lines unique to FILE1)\n");
        printf("  -2                      suppress column 2 (lines unique to FILE2)\n");
        printf("  -3                      suppress column 3 (lines common to both)\n");
        printf("  -i, --ignore-case       ignore case when comparing\n");
        printf("      --check-order       check that the input is correctly sorted\n");
        printf("      --nocheck-order     do not check that the input is correctly sorted\n");
        printf("      --output-delimiter=STR  separate columns with STR\n");
        printf("  -h, --help              display this help and exit\n");
        printf("\n");
        printf("Note: FILE1 and FILE2 must be sorted. Use 'sort' if they are not.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.suppress_col1 = (suppress_1_opt->count > 0);
    opts.suppress_col2 = (suppress_2_opt->count > 0);
    opts.suppress_col3 = (suppress_3_opt->count > 0);
    opts.ignore_case = (ignore_case_opt->count > 0);

    if (nocheck_order_opt->count > 0) {
        opts.check_order = 0;
        opts.nocheck_order = 1;
    } else if (check_order_opt->count > 0) {
        opts.check_order = 1;
    }

    if (output_delim_opt->count > 0) {
        opts.output_delimiter = output_delim_opt->sval[0];
    }

    const char* file1 = file1_arg->filename[0];
    const char* file2 = file2_arg->filename[0];

    int file1_is_stdin = (strcmp(file1, "-") == 0);
    int file2_is_stdin = (strcmp(file2, "-") == 0);

    FileLines f1 = read_file(file1);
    if (f1.lines == NULL && !file1_is_stdin) {
        fprintf(stderr, "comm: %s: No such file or directory\n", file1);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    FileLines f2 = read_file(file2);
    if (f2.lines == NULL && !file2_is_stdin) {
        fprintf(stderr, "comm: %s: No such file or directory\n", file2);
        free_file_lines(f1);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* Check order if requested */
    if (opts.check_order) {
        if (!check_sorted((const char**)f1.lines, f1.count, &opts)) {
            fprintf(stderr, "comm: file 1 is not in sorted order\n");
            free_file_lines(f1);
            free_file_lines(f2);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        if (!check_sorted((const char**)f2.lines, f2.count, &opts)) {
            fprintf(stderr, "comm: file 2 is not in sorted order\n");
            free_file_lines(f1);
            free_file_lines(f2);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    /* Merge-like walk through both files.
     *
     * GNU comm outputs:
     *   col1: lines only in file1
     *   col2: lines only in file2
     *   col3: lines in both
     *
     * Columns are separated by the output delimiter (default tab).
     * Each line of output has up to 3 columns. Trailing delimiters are
     * omitted when later columns are suppressed or empty.
     *
     * Output format per row (when not suppressed):
     *   - Only in file1:        "<line>\n"
     *   - Only in file2:        "\t<line>\n"
     *   - In both:              "\t\t<line>\n"
     *
     * When a column is suppressed, the corresponding delimiter is omitted.
     * E.g., with -1: only in file2 becomes "<line>\n", both becomes "\t<line>\n"
     * With -1 -2: both becomes "<line>\n"
     */

    const char* delim = opts.output_delimiter;
    int i = 0, j = 0;

    while (i < f1.count || j < f2.count) {
        int cmp;

        if (i >= f1.count) {
            cmp = 1; /* f1 exhausted → f2 lines are "greater" */
        } else if (j >= f2.count) {
            cmp = -1; /* f2 exhausted → f1 lines are "greater" */
        } else {
            cmp = compare_lines(f1.lines[i], f2.lines[j], &opts);
        }

        if (cmp < 0) {
            /* Line unique to FILE1 → column 1 */
            if (!opts.suppress_col1) {
                /* Column 1 is first — no leading delimiter needed */
                printf("%s\n", f1.lines[i]);
            }
            i++;
        } else if (cmp > 0) {
            /* Line unique to FILE2 → column 2 */
            if (!opts.suppress_col2) {
                /* Need delimiter(s) before column 2 for any active earlier column */
                int cols_before = 0;
                if (!opts.suppress_col1) cols_before++;
                for (int k = 0; k < cols_before; k++) {
                    printf("%s", delim);
                }
                printf("%s\n", f2.lines[j]);
            }
            j++;
        } else {
            /* Line in both → column 3 */
            if (!opts.suppress_col3) {
                int cols_before = 0;
                if (!opts.suppress_col1) cols_before++;
                if (!opts.suppress_col2) cols_before++;
                for (int k = 0; k < cols_before; k++) {
                    printf("%s", delim);
                }
                printf("%s\n", f1.lines[i]);
            }
            i++;
            j++;
        }
    }

    free_file_lines(f1);
    free_file_lines(f2);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("comm", comm_command, "Compare two sorted files line by line");
