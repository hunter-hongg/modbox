#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <argtable3.h>

#include "commands/tail.hpp"
#include "commands/command_macros.hpp"

/* ── File header printing ──────────────────────────────────────────────── */

static void print_header(const char *fname, FILE *out) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out, "==> %s <==\n", fname);
}

/* ── Ring buffer for last-N-lines mode ─────────────────────────────────── */

typedef struct {
    char **buf;    /* array of N strings */
    int64_t *lens;   /* length of each entry (delimiter included) */
    int64_t size;    /* capacity */
    int64_t pos;     /* next write position */
    int64_t count;   /* number of items written */
} RingBuf;

static RingBuf *ring_new(int64_t n) {
    RingBuf *rb = (RingBuf*)malloc(sizeof(RingBuf));
    rb->buf = (char**)calloc((size_t)n, sizeof(char *));
    rb->lens = (int64_t*)calloc((size_t)n, sizeof(int64_t));
    rb->size = n;
    rb->pos = 0;
    rb->count = 0;
    return rb;
}

static void ring_add(RingBuf *rb, const char *line, int64_t len) {
    if (rb->buf[rb->pos]) free(rb->buf[rb->pos]);
    rb->buf[rb->pos] = (char*)malloc((size_t)len);
    memcpy(rb->buf[rb->pos], line, (size_t)len);
    rb->lens[rb->pos] = len;
    rb->pos = (rb->pos + 1) % rb->size;
    if (rb->count < rb->size) rb->count++;
}

static void ring_flush(RingBuf *rb, FILE *out) {
    int64_t start = (rb->count < rb->size) ? 0 : rb->pos;
    for (int64_t i = 0; i < rb->count; i++) {
        int64_t idx = (start + i) % rb->size;
        if (rb->buf[idx]) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fwrite(rb->buf[idx], 1, (size_t)rb->lens[idx], out);
        }
    }
}

static void ring_free(RingBuf *rb) {
    for (int64_t i = 0; i < rb->size; i++) {
        if (rb->buf[i]) free(rb->buf[i]);
    }
    free(rb->buf);
    free(rb->lens);
    free(rb);
}

/* ── Output last N lines using ring buffer ──────────────────────────────── */

static void tail_lines(FILE *fp, int64_t count, int delim, FILE *out) {
    if (count <= 0) return;

    RingBuf *rb = ring_new(count);
    std::string line;
    line.reserve(256);
    int c;

    while ((c = fgetc(fp)) != EOF) {
        line.push_back((char)c);
        if (c == delim) {
            ring_add(rb, line.c_str(), (int64_t)line.size());
            line.clear();
        }
    }

    /* Emit unterminated tail if file doesn't end with delimiter */
    if (!line.empty()) {
        ring_add(rb, line.c_str(), (int64_t)line.size());
    }

    /* line destructor handles cleanup */
    ring_flush(rb, out);
    ring_free(rb);
}

/* ── Output from line N to end ──────────────────────────────────────────── */

static void tail_lines_from(FILE *fp, int64_t start_line, int delim, FILE *out) {
    int64_t line = 1;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (line >= start_line) (void)fputc(c, out);
        if (c == delim) line++;
    }
}

/* ── Output last N bytes ────────────────────────────────────────────────── */

static void tail_bytes(FILE *fp, int64_t count, FILE *out) {
    if (count <= 0 || fseek(fp, 0, SEEK_END) != 0) return;

    long fsize = ftell(fp);
    if (fsize < 0) return;

    long start = fsize - (long)count;
    if (start < 0) start = 0;

    if (fseek(fp, start, SEEK_SET) != 0) return;

    int c;
    while ((c = fgetc(fp)) != EOF) (void)fputc(c, out);
}

/* ── Output from byte N to end ──────────────────────────────────────────── */

static void tail_bytes_from(FILE *fp, int64_t start_byte, FILE *out) {
    if (fseek(fp, 0, SEEK_SET) != 0) return;
    int64_t pos = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        pos++;
        if (pos >= start_byte) (void)fputc(c, out);
    }
}

/* ── Follow mode (tail -f) ─────────────────────────────────────────────── */

static void follow_file(const char *fname, FILE *fp, int sleep_sec, int retry) {
    /* Get current file size */
    if (fseek(fp, 0, SEEK_END) != 0) return;
    long prev_size = ftell(fp);

    /* Use /dev/inotify when available, otherwise poll */
    for (;;) {
        (void)sleep((unsigned int)sleep_sec);

        /* Check file size */
        if (fseek(fp, 0, SEEK_END) != 0) {
            if (retry) {
                /* File might have been rotated. Try reopening. */
                FILE *new_fp = fopen(fname, "r");
                if (new_fp) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "tail: %s has been replaced; following new file\n", fname);
                    (void)fclose(fp);
                    fp = new_fp;
                    prev_size = 0;
                    continue;
                }
            }
            break;
        }

        long cur_size = ftell(fp);
        if (cur_size < 0) break;

        if (cur_size > prev_size) {
            /* Read new data */
            long pos = prev_size;
            if (fseek(fp, pos, SEEK_SET) != 0) break;
            int c;
            while ((c = fgetc(fp)) != EOF) (void)fputc(c, stdout);
            (void)fflush(stdout);
            prev_size = ftell(fp);
            if (prev_size < 0) break;
        } else if (cur_size < prev_size) {
            /* File truncated */
            prev_size = 0;
        }
    }
}

/* ── Parse -n / -c argument ─────────────────────────────────────────────── */

static int64_t parse_count(const char *s, int *is_relative) {
    *is_relative = 0;
    if (s == NULL) return 10;

    if (s[0] == '+') {
        *is_relative = 1;
        // NOLINTNEXTLINE(cert-err34-c)
        return (int64_t)strtoll(s + 1, NULL, 10);
    }
    // NOLINTNEXTLINE(cert-err34-c)
    int64_t val = (int64_t)strtoll(s, NULL, 10);
    if (val < 0) val = -val;
    return val;
}

/* ── Main command ────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void tail_command(int argc, char **argv) {
    TailOptions opts = {0};

    struct arg_str *lines_opt = arg_str0("n", "lines", "N", "output last N lines (default 10)");
    struct arg_str *bytes_opt = arg_str0("c", "bytes", "N", "output last N bytes");
    struct arg_lit *follow_opt = arg_lit0("f", "follow", "append data as file grows");
    struct arg_lit *follow_retry_opt = arg_lit0("F", NULL, "like -f but retry on file rotation");
    struct arg_lit *quiet_opt = arg_lit0("q", "quiet", "never print headers");
    struct arg_lit *verbose_opt = arg_lit0("v", "verbose", "always print headers");
    struct arg_lit *zero_opt = arg_lit0("z", "zero-terminated", "NUL terminated lines");
    struct arg_int *sleep_opt = arg_int0("s", "sleep-interval", "N", "sleep interval (default 1)");
    struct arg_lit *help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        lines_opt, bytes_opt, follow_opt, follow_retry_opt,
        quiet_opt, verbose_opt, zero_opt, sleep_opt,
        help_opt, file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Print the last 10 lines of each FILE to standard output.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -n, --lines=N         output last N lines (default 10)\n");
        printf("  -c, --bytes=N         output last N bytes\n");
        printf("  -f, --follow          append data as file grows\n");
        printf("  -F                    like -f but retry on file rotation\n");
        printf("  -q, --quiet           never print headers\n");
        printf("  -v, --verbose         always print headers\n");
        printf("  -z, --zero-terminated NUL terminated lines\n");
        printf("  -s, --sleep-interval=N sleep interval (default 1)\n");
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
    opts.follow = (follow_opt->count > 0);
    opts.follow_retry = (follow_retry_opt->count > 0);
    opts.sleep_interval = (sleep_opt->count > 0) ? sleep_opt->ival[0] : 1;
    if (opts.sleep_interval < 1) opts.sleep_interval = 1;
    if (opts.follow_retry) opts.follow = 1;

    /* Parse -n or -c */
    int use_bytes = (bytes_opt->count > 0);
    opts.is_relative = 0;

    if (use_bytes) {
        opts.bytes = parse_count(bytes_opt->sval[0], &opts.is_relative);
        if (opts.bytes == 0) opts.bytes = 10;
        opts.lines = 0;
    } else if (lines_opt->count > 0) {
        opts.lines = parse_count(lines_opt->sval[0], &opts.is_relative);
        if (opts.lines == 0) opts.lines = 10;
    } else {
        opts.lines = 10;
    }

    int delim = opts.zero_terminated ? '\0' : '\n';
    int file_count = file_arg->count;
    int follow_active = opts.follow && file_count > 0;

    if (file_count == 0) {
        /* Read from stdin */
        if (use_bytes) {
            if (opts.is_relative) {
                tail_bytes_from(stdin, opts.bytes, stdout);
            } else {
                tail_bytes(stdin, opts.bytes, stdout);
            }
        } else {
            if (opts.is_relative) {
                tail_lines_from(stdin, opts.lines, delim, stdout);
            } else {
                tail_lines(stdin, opts.lines, delim, stdout);
            }
        }
        if (opts.follow) {
            /* -f with stdin: just exit (no meaningful follow) */
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
                    (void)fprintf(stderr, "tail: %s: %s\n", fname, strerror(errno));
                    continue;
                }
                opened = 1;
            }

            int show_header = 0;
            if (file_count > 1 && !opts.quiet) show_header = 1;
            if (opts.verbose) show_header = 1;

            if (show_header) {
                if (i > 0) (void)fputc('\n', stdout);
                print_header(fname, stdout);
            }

            if (use_bytes) {
                if (opts.is_relative) {
                    tail_bytes_from(fp, opts.bytes, stdout);
                } else {
                    tail_bytes(fp, opts.bytes, stdout);
                }
            } else {
                if (opts.is_relative) {
                    tail_lines_from(fp, opts.lines, delim, stdout);
                } else {
                    tail_lines(fp, opts.lines, delim, stdout);
                }
            }

            if (follow_active && opened) {
                /* Follow mode for this file (only one file supported) */
                follow_file(fname, fp, opts.sleep_interval, opts.follow_retry);
                (void)fclose(fp);
            } else if (opened) {
                (void)fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("tail", tail_command, "Output last part of files");
