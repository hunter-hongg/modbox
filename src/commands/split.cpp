#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include <argtable3.h>

#include "commands/split.hpp"

/* ── Size parsing ─────────────────────────────────────────────────────────── */

static int64_t parse_size(const char *s) {
    if (s == NULL || *s == '\0') return 0;

    // NOLINTNEXTLINE(cert-err34-c)
    int64_t val = (int64_t)strtoll(s, NULL, 10);
    if (val < 0) val = 0;

    const char *p = s;
    while (*p && (*p == '-' || (*p >= '0' && *p <= '9') || *p == '+')) p++;
    if (*p == '\0') return val;

    int64_t mult = 0;
    if (strcmp(p, "K") == 0) mult = 1024LL;
    else if (strcmp(p, "M") == 0) mult = 1024LL * 1024;
    else if (strcmp(p, "G") == 0) mult = 1024LL * 1024 * 1024;
    else if (strcmp(p, "T") == 0) mult = 1024LL * 1024 * 1024 * 1024;
    else if (strcmp(p, "P") == 0) mult = 1024LL * 1024 * 1024 * 1024 * 1024;
    else if (strcmp(p, "E") == 0) mult = 1024LL * 1024 * 1024 * 1024 * 1024 * 1024;
    else if (strcmp(p, "Z") == 0) mult = INT64_MAX;  /* exceeds 64-bit */
    else if (strcmp(p, "Y") == 0) mult = INT64_MAX;  /* exceeds 64-bit */
    else if (strcmp(p, "KiB") == 0) mult = 1024LL;
    else if (strcmp(p, "MiB") == 0) mult = 1024LL * 1024;
    else if (strcmp(p, "GiB") == 0) mult = 1024LL * 1024 * 1024;
    else if (strcmp(p, "TiB") == 0) mult = 1024LL * 1024 * 1024 * 1024;
    else if (strcmp(p, "PiB") == 0) mult = 1024LL * 1024 * 1024 * 1024 * 1024;
    else if (strcmp(p, "EiB") == 0) mult = 1024LL * 1024 * 1024 * 1024 * 1024 * 1024;
    else if (strcmp(p, "ZiB") == 0) mult = INT64_MAX;  /* exceeds 64-bit */
    else if (strcmp(p, "YiB") == 0) mult = INT64_MAX;  /* exceeds 64-bit */
    else if (strcmp(p, "KB") == 0) mult = 1000LL;
    else if (strcmp(p, "MB") == 0) mult = 1000LL * 1000;
    else if (strcmp(p, "GB") == 0) mult = 1000LL * 1000 * 1000;
    else if (strcmp(p, "TB") == 0) mult = 1000LL * 1000 * 1000 * 1000;
    else if (strcmp(p, "PB") == 0) mult = 1000LL * 1000 * 1000 * 1000 * 1000;
    else if (strcmp(p, "EB") == 0) mult = 1000LL * 1000 * 1000 * 1000 * 1000 * 1000;

    if (mult > 0) return val * mult;

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "split: invalid size suffix: %s\n", p);
    return val;
}

/* ── Suffix generation ───────────────────────────────────────────────────── */

static void make_alpha_suffix(char *buf, int len, int64_t index) {
    int64_t count = index;
    int pos = len - 1;
    while (pos >= 0) {
        buf[pos] = (char)('a' + (count % 26));
        count /= 26;
        pos--;
    }
    buf[len] = '\0';
}

static void make_numeric_suffix(char *buf, int len, int64_t index) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(buf, (size_t)(len + 1), "%0*ld", len, (long)index);
}

static void make_hex_suffix(char *buf, int len, int64_t index) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)snprintf(buf, (size_t)(len + 1), "%0*lx", len, (unsigned long)index);
}

/* ── Open output file ─────────────────────────────────────────────────────── */

static FILE *open_output_file(const SplitOptions *opts, const char *prefix,
                              int64_t file_index, char *suffix_buf) {
    if (opts->hex_suffixes)
        make_hex_suffix(suffix_buf, opts->suffix_length, file_index);
    else if (opts->numeric_suffixes)
        make_numeric_suffix(suffix_buf, opts->suffix_length, file_index);
    else
        make_alpha_suffix(suffix_buf, opts->suffix_length, file_index);

    std::string fname = prefix;
    fname += suffix_buf;
    if (opts->additional_suffix)
        fname += opts->additional_suffix;

    FILE *fp = fopen(fname.c_str(), "w");
    if (fp == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "split: cannot open '%s': %s\n",
                      fname.c_str(), strerror(errno));
        return NULL;
    }

    if (opts->verbose) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "creating file '%s'\n", fname.c_str());
    }
    return fp;
}

/* ── Split by lines (-l) ─────────────────────────────────────────────────── */

static void split_by_lines(FILE *in, const SplitOptions *opts,
                           const char *prefix) {
    char suffix_buf[64];
    int64_t file_index = 0;
    int delim = (opts->separator && opts->separator[0])
                    ? opts->separator[0] : '\n';

    FILE *out = open_output_file(opts, prefix, file_index, suffix_buf);
    if (out == NULL) return;

    int64_t line_count = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        (void)fputc(c, out);
        if (c == delim) {
            line_count++;
            if (line_count >= opts->lines) {
                /* Peek ahead to see if there's more input */
                int next = fgetc(in);
                if (next == EOF) break; /* no more data; last file is done */
                (void)fclose(out);
                file_index++;
                out = open_output_file(opts, prefix, file_index, suffix_buf);
                if (out == NULL) return;
                (void)fputc(next, out);
                if (next == delim) line_count = 1;
                else line_count = 0;
            }
        }
    }
    (void)fclose(out);
}

/* ── Split by bytes (-b) ──────────────────────────────────────────────────── */

static void split_by_bytes(FILE *in, const SplitOptions *opts,
                           const char *prefix) {
    char suffix_buf[64];
    int64_t file_index = 0;

    FILE *out = open_output_file(opts, prefix, file_index, suffix_buf);
    if (out == NULL) return;

    int64_t byte_count = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        (void)fputc(c, out);
        byte_count++;
        if (byte_count >= opts->bytes) {
            int next = fgetc(in);
            if (next == EOF) break;
            (void)fclose(out);
            file_index++;
            out = open_output_file(opts, prefix, file_index, suffix_buf);
            if (out == NULL) return;
            (void)fputc(next, out);
            byte_count = 1;
        }
    }
    (void)fclose(out);
}

/* ── Split by line-bytes (-C) ─────────────────────────────────────────────── */

static void split_by_line_bytes(FILE *in, const SplitOptions *opts,
                                const char *prefix) {
    char suffix_buf[64];
    int64_t file_index = 0;
    int delim = (opts->separator && opts->separator[0])
                    ? opts->separator[0] : '\n';

    FILE *out = open_output_file(opts, prefix, file_index, suffix_buf);
    if (out == NULL) return;

    int64_t byte_count = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        (void)fputc(c, out);
        byte_count++;
        if (c == delim && byte_count >= opts->line_bytes) {
            int next = fgetc(in);
            if (next == EOF) break;
            (void)fclose(out);
            file_index++;
            out = open_output_file(opts, prefix, file_index, suffix_buf);
            if (out == NULL) return;
            (void)fputc(next, out);
            byte_count = 1;
        }
    }
    (void)fclose(out);
}

/* ── Chunk helpers ───────────────────────────────────────────────────────── */

static int64_t count_bytes_file(FILE *in) {
    int64_t total = 0;
    int c;
    while ((c = fgetc(in)) != EOF) total++;
    (void)clearerr(in);
    (void)fseek(in, 0, SEEK_SET);
    return total;
}

static int64_t count_lines_file(FILE *in) {
    int64_t total = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '\n') total++;
    }
    (void)clearerr(in);
    (void)fseek(in, 0, SEEK_SET);
    return total;
}

static void split_n_chunks(FILE *in, int64_t total_bytes,
                           const SplitOptions *opts,
                           const char *prefix, int nchunks) {
    if (nchunks < 1) return;

    int64_t chunk_size = total_bytes / nchunks;
    int64_t remainder = total_bytes % nchunks;

    char suffix_buf[64];
    for (int i = 0; i < nchunks; i++) {
        int64_t this_size = chunk_size + (i < remainder ? 1 : 0);
        FILE *out = open_output_file(opts, prefix, i, suffix_buf);
        if (out == NULL) return;

        int64_t written = 0;
        int c;
        while (written < this_size && (c = fgetc(in)) != EOF) {
            (void)fputc(c, out);
            written++;
        }
        (void)fclose(out);

        if (opts->elide_empty && this_size == 0) {
            std::string fname = prefix;
            fname += suffix_buf;
            if (opts->additional_suffix) fname += opts->additional_suffix;
            (void)remove(fname.c_str());
        }
    }
}

static void split_n_lines(FILE *in, int64_t total_lines,
                          const SplitOptions *opts,
                          const char *prefix, int nchunks) {
    if (nchunks < 1) return;

    int64_t per_file = total_lines / nchunks;
    int64_t remainder = total_lines % nchunks;
    if (per_file == 0 && remainder > 0) per_file = 1;

    char suffix_buf[64];
    for (int i = 0; i < nchunks; i++) {
        int64_t this_lines = per_file + (i < remainder ? 1 : 0);
        if (this_lines == 0 && opts->elide_empty) continue;

        FILE *out = open_output_file(opts, prefix, i, suffix_buf);
        if (out == NULL) return;

        int64_t written = 0;
        int c;
        while (written < this_lines && (c = fgetc(in)) != EOF) {
            (void)fputc(c, out);
            if (c == '\n') written++;
        }
        (void)fclose(out);
    }
}

static void split_round_robin(FILE *in, const SplitOptions *opts,
                              const char *prefix, int nchunks) {
    if (nchunks < 1) return;

    char suffix_buf[64];
    std::vector<FILE *> files;
    files.reserve((size_t)nchunks);

    for (int i = 0; i < nchunks; i++) {
        FILE *out = open_output_file(opts, prefix, i, suffix_buf);
        if (out == NULL) {
            for (auto fp : files) (void)fclose(fp);
            return;
        }
        files.push_back(out);
    }

    int idx = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        (void)fputc(c, files[(size_t)idx]);
        if (c == '\n') idx = (idx + 1) % nchunks;
    }

    for (auto fp : files) (void)fclose(fp);
}

/* ── Main command ─────────────────────────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void split_command(int argc, char **argv) {
    SplitOptions opts = {0};

    struct arg_lit *numeric_opt = arg_lit0("d", "numeric-suffixes",
                                           "use numeric suffixes (00, 01, ...)");
    struct arg_lit *hex_opt = arg_lit0("x", "hex-suffixes",
                                       "use hexadecimal suffixes");
    struct arg_int *suffix_len_opt = arg_int0("a", "suffix-length", "N",
                                              "suffix length (default 2)");
    struct arg_str *lines_opt = arg_str0("l", "lines", "N",
                                         "put N lines per output file");
    struct arg_str *bytes_opt = arg_str0("b", "bytes", "SIZE",
                                         "put SIZE bytes per output file");
    struct arg_str *line_bytes_opt = arg_str0("C", "line-bytes", "SIZE",
                                              "put at most SIZE bytes of lines");
    struct arg_str *chunks_opt = arg_str0("n", "number", "CHUNKS",
                                          "generate CHUNKS output files");
    struct arg_str *additional_suffix_opt = arg_str0(NULL, "additional-suffix",
                                                     "SUFFIX",
                                                     "append SUFFIX to output names");
    struct arg_str *filter_opt = arg_str0(NULL, "filter", "COMMAND",
                                          "write to shell command");
    struct arg_str *separator_opt = arg_str0("t", "separator", "SEP",
                                             "use SEP as line separator");
    struct arg_lit *unbuffered_opt = arg_lit0("u", "unbuffered",
                                              "immediately copy input to output");
    struct arg_lit *verbose_opt = arg_lit0(NULL, "verbose",
                                           "print diagnostic before each output file");
    struct arg_lit *elide_opt = arg_lit0("e", "elide-empty-files",
                                         "do not generate empty output files");
    struct arg_lit *help_opt = arg_lit0("h", "help",
                                        "display this help and exit");
    struct arg_file *file_arg = arg_filen(NULL, NULL, "FILE", 0, 2,
                                          "input file (stdin if omitted) and prefix");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        numeric_opt, hex_opt, suffix_len_opt,
        lines_opt, bytes_opt, line_bytes_opt, chunks_opt,
        additional_suffix_opt, filter_opt, separator_opt,
        unbuffered_opt, verbose_opt, elide_opt,
        help_opt, file_arg, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE [PREFIX]]\n", argv[0]);
        printf("Split a file into pieces.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -a, --suffix-length=N   suffix length (default 2)\n");
        printf("  -b, --bytes=SIZE        put SIZE bytes per output file\n");
        printf("  -C, --line-bytes=SIZE   put at most SIZE bytes of lines\n");
        printf("  -d, --numeric-suffixes  use numeric suffixes (00, 01, ...)\n");
        printf("  -e, --elide-empty-files do not generate empty output files\n");
        printf("  -l, --lines=N           put N lines per output file (default 1000)\n");
        printf("  -n, --number=CHUNKS     generate CHUNKS output files\n");
        printf("  -t, --separator=SEP     use SEP as line separator\n");
        printf("  -u, --unbuffered        immediately copy input to output\n");
        printf("  -x, --hex-suffixes      use hexadecimal suffixes\n");
        printf("  -h, --help              display this help and exit\n");
        printf("      --additional-suffix=SUFFIX  append SUFFIX to output names\n");
        printf("      --filter=COMMAND    write to shell command\n");
        printf("      --verbose           print diagnostic before each output file\n");
        printf("\n");
        printf("SIZE suffixes:\n");
        printf("  K=1024, M=1024^2, G=1024^3, T=1024^4, P=1024^5, E=1024^6\n");
        printf("  KB=1000, MB=1000^2, GB=1000^3, TB=1000^4\n");
        printf("  KiB=1024, MiB=1024^2, GiB=1024^3, TiB=1024^4\n");
        printf("\n");
        printf("CHUNKS may be:\n");
        printf("  N       split into N files based on size of input\n");
        printf("  l/N     split into N files without splitting lines\n");
        printf("  r/N     like l/N but use round robin distribution\n");
        printf("\n");
        printf("PREFIX is the output file prefix (default \"x\").\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* Collect options */
    opts.numeric_suffixes = (numeric_opt->count > 0);
    opts.hex_suffixes = (hex_opt->count > 0);
    opts.verbose = (verbose_opt->count > 0);
    opts.elide_empty = (elide_opt->count > 0);
    opts.unbuffered = (unbuffered_opt->count > 0);

    if (suffix_len_opt->count > 0) {
        opts.suffix_length = suffix_len_opt->ival[0];
        if (opts.suffix_length < 1) opts.suffix_length = 1;
    }

    if (additional_suffix_opt->count > 0)
        opts.additional_suffix = additional_suffix_opt->sval[0];
    if (filter_opt->count > 0)
        opts.filter = filter_opt->sval[0];
    if (separator_opt->count > 0)
        opts.separator = separator_opt->sval[0];
    if (chunks_opt->count > 0)
        opts.chunks = chunks_opt->sval[0];

    /* Parse SIZE options */
    int use_lines = 0, use_bytes = 0, use_line_bytes = 0, use_chunks = 0;

    if (bytes_opt->count > 0) {
        opts.bytes = parse_size(bytes_opt->sval[0]);
        if (opts.bytes > 0) use_bytes = 1;
    }
    if (line_bytes_opt->count > 0) {
        opts.line_bytes = parse_size(line_bytes_opt->sval[0]);
        if (opts.line_bytes > 0) use_line_bytes = 1;
    }
    if (lines_opt->count > 0) {
        // NOLINTNEXTLINE(cert-err34-c)
        opts.lines = (int64_t)strtoll(lines_opt->sval[0], NULL, 10);
        if (opts.lines > 0) use_lines = 1;
    }
    if (opts.chunks != NULL) use_chunks = 1;

    /* Default: 1000 lines */
    if (!use_bytes && !use_line_bytes && !use_chunks && !use_lines) {
        use_lines = 1;
        opts.lines = 1000;
    }

    /* Determine input file and prefix from positional arguments.
     * file_arg captures [FILE [PREFIX]] — up to 2 positionals. */
    const char *prefix = "x";
    FILE *in = stdin;
    int opened = 0;

    if (file_arg->count > 0) {
        const char *input_name = file_arg->filename[0];
        if (strcmp(input_name, "-") != 0) {
            in = fopen(input_name, "r");
            if (in == NULL) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "split: %s: %s\n", input_name, strerror(errno));
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
            opened = 1;
        }
    }

    /* Second positional is the prefix */
    if (file_arg->count >= 2)
        prefix = file_arg->filename[1];

    /* --filter not implemented */
    if (opts.filter != NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "split: --filter is not yet implemented\n");
        if (opened) (void)fclose(in);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* Dispatch */
    if (use_chunks) {
        const char *cs = opts.chunks;
        int n = 0;
        // NOLINTNEXTLINE(cert-err34-c)
        if (cs[0] == 'l' && cs[1] == '/') {
            n = (int)strtol(cs + 2, NULL, 10);
            if (n < 1) goto bad_chunks;
            split_n_lines(in, count_lines_file(in), &opts, prefix, n);
        } else if (cs[0] == 'r' && cs[1] == '/') {
            n = (int)strtol(cs + 2, NULL, 10);
            if (n < 1) goto bad_chunks;
            split_round_robin(in, &opts, prefix, n);
        } else {
            n = (int)strtol(cs, NULL, 10);
            if (n < 1) goto bad_chunks;
            split_n_chunks(in, count_bytes_file(in), &opts, prefix, n);
        }
    } else if (use_bytes) {
        split_by_bytes(in, &opts, prefix);
    } else if (use_line_bytes) {
        split_by_line_bytes(in, &opts, prefix);
    } else {
        split_by_lines(in, &opts, prefix);
    }

    if (opened) (void)fclose(in);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;

bad_chunks:
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "split: invalid number of chunks: %s\n", opts.chunks);
    if (opened) (void)fclose(in);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
