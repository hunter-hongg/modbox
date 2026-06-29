#include <cstdio>
#include <cstring>
#include <cctype>
#include <argtable3.h>

#include "commands/uniq.hpp"

#define UNIQ_MAX_LINE 1048576  /* 1 MiB max line length */

/* Compare two lines according to options.
 * Returns 0 if equal, nonzero otherwise. */
static int lines_equal(const char* a, const char* b, const UniqOptions* opts) {
    const char* pa = a;
    const char* pb = b;

    /* Skip fields (whitespace-delimited) */
    if (opts->skip_fields > 0) {
        int f;
        for (f = 0; f < opts->skip_fields; f++) {
            /* Skip whitespace */
            while (*pa && (unsigned char)*pa <= ' ') { pa++; }
            while (*pb && (unsigned char)*pb <= ' ') { pb++; }
            /* Skip non-whitespace */
            while (*pa && (unsigned char)*pa > ' ') { pa++; }
            while (*pb && (unsigned char)*pb > ' ') { pb++; }
        }
    }

    /* Skip characters */
    {
        size_t la = strlen(pa);
        size_t lb = strlen(pb);
        int sc = opts->skip_chars;
        pa += (sc < (int)la) ? sc : (int)la;
        pb += (sc < (int)lb) ? sc : (int)lb;
    }

    /* Determine compare length */
    size_t max_cmp = opts->check_chars > 0 ? (size_t)opts->check_chars : (size_t)-1;

    if (opts->ignore_case) {
        size_t n = 0;
        while (*pa && *pb && n < max_cmp) {
            if (std::tolower((unsigned char)*pa) != std::tolower((unsigned char)*pb)) {
                return 1;
            }
            pa++;
            pb++;
            n++;
        }
        /* If one is shorter but within compare window, they differ only if one ended */
        if (n < max_cmp) {
            return (*pa != *pb) ? 1 : 0;
        }
        return 0;
    }

    size_t n = 0;
    while (*pa && *pb && n < max_cmp) {
        if (*pa != *pb) {
            return 1;
        }
        pa++;
        pb++;
        n++;
    }
    if (n < max_cmp) {
        return (*pa != *pb) ? 1 : 0;
    }
    return 0;
}

/* Output a group of lines according to options. */
static void output_group(FILE* out_fp, const UniqOptions* opts, const char* line, int count) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    int is_dup = (count > 1);
    if (opts->count) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out_fp, "%7d %s\n", count, line);
    } else if (opts->all_repeated) {
        if (is_dup) {
            for (int i = 0; i < count; i++) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(out_fp, "%s\n", line);
            }
        }
    } else if (opts->repeated) {
        if (is_dup) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out_fp, "%s\n", line);
        }
    } else if (opts->unique) {
        if (!is_dup) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(out_fp, "%s\n", line);
        }
    } else {
        /* Default: print each group once */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out_fp, "%s\n", line);
    }
}

/* Process input file, write uniq output. */
static void uniq_file(FILE* fp, const UniqOptions* opts, FILE* out_fp) {
    char line_buf[UNIQ_MAX_LINE];
    char prev_buf[UNIQ_MAX_LINE];
    int has_prev = 0;
    int count = 1;

    while (fgets(line_buf, UNIQ_MAX_LINE, fp)) {
        size_t len = strlen(line_buf);
        /* Strip trailing newline */
        if (len > 0 && line_buf[len - 1] == '\n') {
            line_buf[len - 1] = '\0';
        }

        if (!has_prev) {
            memcpy(prev_buf, line_buf, UNIQ_MAX_LINE);
            has_prev = 1;
            count = 1;
            continue;
        }

        if (lines_equal(prev_buf, line_buf, opts) == 0) {
            count++;
        } else {
            output_group(out_fp, opts, prev_buf, count);
            memcpy(prev_buf, line_buf, UNIQ_MAX_LINE);
            count = 1;
        }
    }

    /* Output last group */
    if (has_prev) {
        output_group(out_fp, opts, prev_buf, count);
    }
}

/* ── Main command ────────────────────────────────────────────────────────── */

void uniq_command(int argc, char** argv) {
    UniqOptions opts = {0};

    struct arg_lit* count_opt = arg_lit0("c", "count", "prefix lines by the number of occurrences");
    struct arg_lit* repeated_opt = arg_lit0("d", "repeated", "only print duplicate lines, one per group");
    struct arg_lit* all_repeated_opt = arg_lit0("D", "all-repeated", "print all duplicate lines");
    struct arg_lit* unique_opt = arg_lit0("u", "unique", "only print unique lines");
    struct arg_lit* ignore_case_opt = arg_lit0("i", "ignore-case", "ignore case when comparing");
    struct arg_int* skip_fields_opt = arg_int0("f", "skip-fields", "N", "skip N fields");
    struct arg_int* skip_chars_opt = arg_int0("s", "skip-chars", "N", "skip N characters");
    struct arg_int* check_chars_opt = arg_int0("w", "check-chars", "N", "compare no more than N characters per line");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "INPUT", 0, 1, "input file (default: stdin)");
    struct arg_file* output_arg = arg_filen(NULL, NULL, "OUTPUT", 0, 1, "output file (default: stdout)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        count_opt, repeated_opt, all_repeated_opt, unique_opt,
        ignore_case_opt, skip_fields_opt, skip_chars_opt, check_chars_opt,
        help_opt,
        file_arg, output_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [INPUT [OUTPUT]]\n", argv[0]);
        printf("Filter adjacent matching lines from INPUT (or stdin), writing to OUTPUT (or stdout).\n");
        printf("\n");
        printf("With no options, matching lines are merged to the first occurrence.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -c, --count           prefix lines by the number of occurrences\n");
        printf("  -d, --repeated        only print duplicate lines, one per group\n");
        printf("  -D, --all-repeated    print all duplicate lines\n");
        printf("  -u, --unique          only print unique lines\n");
        printf("  -i, --ignore-case     ignore case when comparing\n");
        printf("  -f, --skip-fields=N   avoid comparing the first N fields\n");
        printf("  -s, --skip-chars=N    avoid comparing the first N characters\n");
        printf("  -w, --check-chars=N   compare no more than N characters per line\n");
        printf("  -h, --help            display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.count = (count_opt->count > 0);
    opts.repeated = (repeated_opt->count > 0);
    opts.all_repeated = (all_repeated_opt->count > 0);
    opts.unique = (unique_opt->count > 0);
    opts.ignore_case = (ignore_case_opt->count > 0);
    opts.skip_fields = (skip_fields_opt->count > 0 ? skip_fields_opt->ival[0] : 0);
    opts.skip_chars = (skip_chars_opt->count > 0 ? skip_chars_opt->ival[0] : 0);
    opts.check_chars = (check_chars_opt->count > 0 ? check_chars_opt->ival[0] : 0);

    /* Determine input and output */
    FILE* in_fp = stdin;
    FILE* out_fp = stdout;

    if (file_arg->count > 0) {
        const char* fname = file_arg->filename[0];
        if (strcmp(fname, "-") != 0) {
            in_fp = fopen(fname, "r");
            if (in_fp == NULL) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "uniq: %s: No such file or directory\n", fname);
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
        }
    }

    if (output_arg->count > 0) {
        const char* fname = output_arg->filename[0];
        out_fp = fopen(fname, "w");
        if (out_fp == NULL) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "uniq: %s: Cannot open for writing\n", fname);
            if (in_fp != stdin) { (void)fclose(in_fp); }
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    uniq_file(in_fp, &opts, out_fp);

    if (in_fp != stdin) { (void)fclose(in_fp); }
    if (out_fp != stdout) { (void)fclose(out_fp); }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
