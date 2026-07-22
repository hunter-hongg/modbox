#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <argtable3.h>

#include "commands/head.hpp"
#include "commands/command_macros.hpp"

/* ── File header printing ──────────────────────────────────────────────── */

static void print_header(const char *fname, FILE *out) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out, "==> %s <==\n", fname);
}

/* ── Head by lines ──────────────────────────────────────────────────────── */

static int64_t head_lines(FILE *fp, int64_t count, int delim, FILE *out) {
    int64_t printed = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        (void)fputc(c, out);
        if (c == delim) {
            printed++;
            if (printed >= count) break;
        }
    }
    /* If last line has no terminator but we haven't hit limit, it's fine */
    return printed;
}

static int64_t head_lines_file(const char *fname, FILE *fp, int64_t count,
                               int delim, const HeadOptions *opts,
                               int file_index, int total_files, FILE *out) {
    int show_header = 0;
    if (total_files > 1 && !opts->quiet) show_header = 1;
    if (opts->verbose) show_header = 1;

    if (show_header) {
        if (file_index > 0) (void)fputc('\n', out);
        print_header(fname, out);
    }

    return head_lines(fp, count, delim, out);
}

/* ── Head by bytes ──────────────────────────────────────────────────────── */

static int64_t head_bytes(FILE *fp, int64_t count, FILE *out) {
    int64_t printed = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && printed < count) {
        (void)fputc(c, out);
        printed++;
    }
    return printed;
}

static int64_t head_bytes_file(const char *fname, FILE *fp, int64_t count,
                               const HeadOptions *opts,
                               int file_index, int total_files, FILE *out) {
    int show_header = 0;
    if (total_files > 1 && !opts->quiet) show_header = 1;
    if (opts->verbose) show_header = 1;

    if (show_header) {
        if (file_index > 0) (void)fputc('\n', out);
        print_header(fname, out);
    }

    return head_bytes(fp, count, out);
}

/* ── Parse -n / -c argument ─────────────────────────────────────────────── */

/* Handle formats: N, -N (abs), +N. Returns parsed value.
   For head: -n N = first N lines; -n +N = skip N-1 lines, show rest */
static int64_t parse_count(const char *s, int *is_relative) {
    *is_relative = 0;
    if (s == NULL) return 10;

    if (s[0] == '+') {
        *is_relative = 1;
        // NOLINTNEXTLINE(cert-err34-c)
        return (int64_t)strtoll(s + 1, NULL, 10);
    }
    // NOLINTNEXTLINE(cert-err34-c)
    return (int64_t)strtoll(s, NULL, 10);
}

/* ── Main command ────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void head_command(int argc, char **argv) {
    HeadOptions opts = {0};

    struct arg_str *lines_opt = arg_str0("n", "lines", "N", "print first N lines (default 10)");
    struct arg_str *bytes_opt = arg_str0("c", "bytes", "N", "print first N bytes");
    struct arg_lit *quiet_opt = arg_lit0("q", "quiet", "never print headers");
    struct arg_lit *verbose_opt = arg_lit0("v", "verbose", "always print headers");
    struct arg_lit *zero_opt = arg_lit0("z", "zero-terminated", "NUL terminated lines");
    struct arg_lit *help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        lines_opt, bytes_opt, quiet_opt, verbose_opt, zero_opt,
        help_opt, file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Print the first 10 lines of each FILE to standard output.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -n, --lines=N         print first N lines (default 10)\n");
        printf("  -c, --bytes=N         print first N bytes\n");
        printf("  -q, --quiet           never print headers\n");
        printf("  -v, --verbose         always print headers\n");
        printf("  -z, --zero-terminated NUL terminated lines\n");
        printf("  -h, --help            display this help and exit\n");
        printf("\n");
        printf("With no FILE, read standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    opts.quiet = (quiet_opt->count > 0);
    opts.verbose = (verbose_opt->count > 0);
    opts.zero_terminated = (zero_opt->count > 0);

    /* Parse -n or -c */
    int use_bytes = (bytes_opt->count > 0);
    int is_relative = 0;

    if (use_bytes) {
        opts.bytes = parse_count(bytes_opt->sval[0], &is_relative);
        if (opts.bytes < 0) opts.bytes = -opts.bytes;
        if (opts.bytes == 0) opts.bytes = 10;
        opts.lines = 0;
    } else if (lines_opt->count > 0) {
        opts.lines = parse_count(lines_opt->sval[0], &is_relative);
        if (opts.lines < 0) opts.lines = -opts.lines;
        if (opts.lines == 0) opts.lines = 10;
    } else {
        opts.lines = 10;
    }

    int delim = opts.zero_terminated ? '\0' : '\n';
    int file_count = file_arg->count;

    if (file_count == 0) {
        /* Read from stdin */
        if (use_bytes) {
            head_bytes(stdin, opts.bytes, stdout);
        } else if (is_relative) {
            /* -n +N: skip N-1 lines then show rest */
            int64_t skip = opts.lines - 1;
            while (skip > 0) {
                int c = fgetc(stdin);
                if (c == EOF) break;
                if (c == delim) skip--;
            }
            /* Now output remaining */
            int c;
            while ((c = fgetc(stdin)) != EOF) (void)fputc(c, stdout);
        } else {
            head_lines(stdin, opts.lines, delim, stdout);
        }
    } else {
        for (int i = 0; i < file_count; i++) {
            const char *fname = file_arg->filename[i];
            FILE *fp = stdin;
            int opened = 0;

            if (strcmp(fname, "-") == 0) {
                fp = stdin;
            } else {
                fp = fopen(fname, "r");
                if (fp == NULL) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "head: %s: %s\n", fname, strerror(errno));
                    continue;
                }
                opened = 1;
            }

            if (use_bytes) {
                head_bytes_file(fname, fp, opts.bytes, &opts, i, file_count, stdout);
            } else if (is_relative) {
                /* -n +N: skip N-1, show rest */
                int show_header = 0;
                if (file_count > 1 && !opts.quiet) show_header = 1;
                if (opts.verbose) show_header = 1;

                int64_t skip = opts.lines - 1;
                while (skip > 0) {
                    int c = fgetc(fp);
                    if (c == EOF) break;
                    if (c == delim) skip--;
                }

                /* Check if any content remains after skipping */
                if (!feof(fp)) {
                    if (show_header) {
                        if (i > 0) (void)fputc('\n', stdout);
                        print_header(fname, stdout);
                    }
                    int c;
                    while ((c = fgetc(fp)) != EOF) (void)fputc(c, stdout);
                }
            } else {
                head_lines_file(fname, fp, opts.lines, delim, &opts, i, file_count, stdout);
            }

            if (opened) (void)fclose(fp);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("head", head_command, "Output first part of files");
