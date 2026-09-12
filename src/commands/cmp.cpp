#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <argtable3.h>

#include "commands/cmp.hpp"
#include "commands/command_macros.hpp"
#include "commands/arg_util.hpp"
#include "commands/cmd_error.hpp"
#include "commands/version_util.hpp"

/* ── Constants ──────────────────────────────────────────────────────────── */

#define CMP_BUF_SIZE 65536  /* 64 KiB read chunks */

/* Exit status, per GNU cmp / diffutils. */
#define CMP_EQUAL    0
#define CMP_DIFFER   1
#define CMP_ERROR    2

namespace {

/* ── Mnemonic rendering for -b ──────────────────────────────────────────── */

/* Appends the GNU "print-bytes" mnemonic for one octet to buffer.
 *
 * Rules (matched against GNU diffutils):
 *   - printable (0x20..0x7e): the literal character, including space
 *   - control characters: '^' followed by the character with bit 6 set
 *   - DEL (0x7f): "^?"
 *   - bytes with the high bit set: "M-" prefix, then the low 7 bits rendered
 *     by the same control/printable rule (e.g. 0xff -> "M-^?", 0xfe -> "M-~")
 * The result is at most 4 characters wide. */
void format_mnemonic(unsigned char c, char* out, size_t out_size) {
    size_t n = 0;
    auto put = [&](char ch) {
        if (n + 1 < out_size) {
            out[n++] = ch;
        }
    };

    unsigned char v = c;
    if ((c & 0x80U) != 0U) {
        put('M');
        put('-');
        v = static_cast<unsigned char>(c & 0x7fU);
    }

    if (v == 0x7fU) {
        put('^');
        put('?');
    } else if (v < 0x20U) {
        put('^');
        put(static_cast<char>(v + 0x40U));
    } else {
        put(static_cast<char>(v));
    }

    out[n] = '\0';
}

/* ── Byte stream over a file or stdin ───────────────────────────────────── */

struct ByteStream {
    FILE* fp = nullptr;
    bool owns_fp = false;
    unsigned char buf[CMP_BUF_SIZE];
    size_t len = 0;   /* valid bytes in buf */
    size_t pos = 0;   /* next unconsumed byte index */
    int64_t consumed = 0;  /* bytes returned by next() */
    int last_byte = '\n';  /* most recent byte returned, '\n' until one is read */

    void close() {
        if (owns_fp && fp != nullptr) {
            (void)fclose(fp);
            owns_fp = false;
        }
        fp = nullptr;
    }

    /* Reads and returns the next byte, or -1 at end of input. */
    int next() {
        if (pos >= len) {
            if (fp == nullptr) {
                return -1;
            }
            /* fp is owned by this struct and released by close(), which
             * compare_inputs() calls on every return path; the analyzer
             * cannot see that far across the call boundary. */
            // NOLINTNEXTLINE(clang-analyzer-unix.Stream)
            len = fread(buf, 1, sizeof(buf), fp);
            pos = 0;
            if (len == 0) {
                return -1;
            }
        }
        consumed++;
        last_byte = static_cast<int>(buf[pos]);
        return static_cast<int>(buf[pos++]);
    }

    /* Discards up to `count` bytes, returning how many were actually skipped.
     * Skipped bytes are not counted as consumed so that byte numbers and EOF
     * byte counts match GNU cmp, which numbers from the first compared byte. */
    int64_t skip(int64_t count) {
        int64_t skipped = 0;
        while (skipped < count) {
            if (next() < 0) {
                break;
            }
            skipped++;
        }
        consumed -= skipped;
        return skipped;
    }
};

/* ── Line tracking ──────────────────────────────────────────────────────── */

/* Tracks the 1-based line number of the next byte to be read from a stream.
 * GNU cmp counts '\n' as ending a line, so a difference occurring right
 * after a newline is reported on the following line. */
struct LineCounter {
    int64_t line = 1;
    void observe(int byte) {
        if (byte == '\n') {
            line++;
        }
    }
};

/* ── Argument helpers ───────────────────────────────────────────────────── */

/* Parses a non-negative decimal integer, or -1 on malformed input. */
int64_t parse_count(const char* text) {
    if (text == nullptr || *text == '\0') {
        return -1;
    }
    char* endp = nullptr;
    long long const value = strtoll(text, &endp, 10);
    if (endp == text || *endp != '\0' || value < 0) {
        return -1;
    }
    return static_cast<int64_t>(value);
}

/* Splits "SKIP" or "SKIP1:SKIP2" from --ignore-initial into two values. */
int parse_ignore_initial(const char* text, CmpOptions* opts) {
    const char* colon = strchr(text, ':');
    if (colon == nullptr) {
        int64_t const skip = parse_count(text);
        if (skip < 0) {
            return -1;
        }
        opts->ignore_initial1 = skip;
        opts->ignore_initial2 = skip;
        return 0;
    }

    char first[64];
    size_t const first_len = static_cast<size_t>(colon - text);
    if (first_len >= sizeof(first)) {
        return -1;
    }
    memcpy(first, text, first_len);
    first[first_len] = '\0';

    int64_t const skip1 = parse_count(first);
    int64_t const skip2 = parse_count(colon + 1);
    if (skip1 < 0 || skip2 < 0) {
        return -1;
    }
    opts->ignore_initial1 = skip1;
    opts->ignore_initial2 = skip2;
    return 0;
}

/* ── Help ───────────────────────────────────────────────────────────────── */

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... FILE1 [FILE2 [SKIP1 [SKIP2]]]\n", prog);
    printf("Compare two files byte by byte.\n");
    printf("\n");
    printf("With no FILE2, or when a FILE is '-', read standard input.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -b, --print-bytes          print differing bytes in octal and mnemonic\n");
    printf("  -i, --ignore-initial=SKIP  skip the first SKIP bytes of both inputs\n");
    printf("      --ignore-initial=SKIP1:SKIP2  skip SKIP1/SKIP2 for FILE1/FILE2\n");
    printf("  -l, --verbose              output byte numbers and differing values\n");
    printf("  -n, --bytes=LIMIT          compare at most LIMIT bytes\n");
    printf("  -s, --quiet, --silent      suppress all normal output\n");
    printf("  -h, --help                 display this help and exit\n");
    printf("  -V, --version              output version information and exit\n");
    printf("\n");
    printf("Exit status is 0 if inputs are the same, 1 if different, 2 if trouble.\n");
}

/* ── Comparison output ─────────────────────────────────────────────────── */

/* -l: one line per differing byte: offset, then the octal values. With -b the
 * mnemonics are inserted, left-justified in a 4-column field, matching GNU. */
void print_listed_difference(int64_t byte_number, int a, int b, bool with_mnemonics) {
    if (with_mnemonics) {
        char m1[8];
        char m2[8];
        format_mnemonic(static_cast<unsigned char>(a), m1, sizeof(m1));
        format_mnemonic(static_cast<unsigned char>(b), m2, sizeof(m2));
        printf("%lld %3o %-5s%3o %s\n",
               static_cast<long long>(byte_number),
               static_cast<unsigned>(a) & 0xFFU, m1,
               static_cast<unsigned>(b) & 0xFFU, m2);
    } else {
        printf("%lld %3o %3o\n",
               static_cast<long long>(byte_number),
               static_cast<unsigned>(a) & 0xFFU,
               static_cast<unsigned>(b) & 0xFFU);
    }
}

/* Default mode: the classic "FILE1 FILE2 differ: byte B, line L" report,
 * with trailing byte details when -b is in effect. */
void print_first_difference(const char* name1, const char* name2,
                            int64_t byte_number, int64_t line_number,
                            int a, int b, bool with_mnemonics) {
    printf("%s %s differ: byte %lld, line %lld",
           name1, name2, static_cast<long long>(byte_number),
           static_cast<long long>(line_number));
    if (with_mnemonics) {
        char m1[8];
        char m2[8];
        format_mnemonic(static_cast<unsigned char>(a), m1, sizeof(m1));
        format_mnemonic(static_cast<unsigned char>(b), m2, sizeof(m2));
        printf(" is %3o %s %3o %s",
               static_cast<unsigned>(a) & 0xFFU, m1,
               static_cast<unsigned>(b) & 0xFFU, m2);
    }
    printf("\n");
}

/* Reports that one input ended before the other, naming the short file and
 * how much of it was read. Byte listings (-l) drop the line suffix because
 * every listed line already carries an offset. */
void print_eof_diagnostic(const char* prog, const char* short_name,
                          int64_t eof_consumed, int64_t eof_line,
                          int eof_last_byte, bool verbose) {
    if (eof_consumed == 0) {
        (void)fprintf(stderr, "%s: EOF on \xe2\x80\x98%s\xe2\x80\x99 which is empty\n",
                      prog, short_name);
    } else if (verbose) {
        /* Byte listings already carry an offset per line, so GNU omits the
         * line information from the note in that mode. */
        (void)fprintf(stderr, "%s: EOF on \xe2\x80\x98%s\xe2\x80\x99 after byte %lld\n",
                      prog, short_name, static_cast<long long>(eof_consumed));
    } else if (eof_last_byte == '\n') {
        (void)fprintf(stderr, "%s: EOF on \xe2\x80\x98%s\xe2\x80\x99 after byte %lld, line %lld\n",
                      prog, short_name, static_cast<long long>(eof_consumed),
                      static_cast<long long>(eof_line));
    } else {
        (void)fprintf(stderr, "%s: EOF on \xe2\x80\x98%s\xe2\x80\x99 after byte %lld, in line %lld\n",
                      prog, short_name, static_cast<long long>(eof_consumed),
                      static_cast<long long>(eof_line));
    }
}

/* Accumulates the first difference and, for -l, streams every difference. */
struct DiffReport {
    bool differ = false;
    int64_t first_byte = 0;
    int64_t first_line = 0;
    int first_a = 0;
    int first_b = 0;

    /* Records a difference at `byte_number` on `line`; prints it when -l is
     * active and anything at all when this is the first one seen. */
    void note(int64_t byte_number, int64_t line, int a, int b, bool verbose, bool print_bytes) {
        if (!differ) {
            differ = true;
            first_byte = byte_number;
            first_line = line;
            first_a = a;
            first_b = b;
        }
        if (verbose) {
            print_listed_difference(byte_number, a, b, print_bytes);
        }
    }
};

/* Opens one side of the comparison; '-' (or an omitted operand) is stdin. */
FILE* open_side(const char* name, bool* owns) {
    if (strcmp(name, "-") == 0) {
        *owns = false;
        return stdin;
    }
    *owns = true;
    return fopen(name, "rb");
}

/* ── Comparison engine ─────────────────────────────────────────────────── */

/* Runs the byte-by-byte comparison and prints GNU-compatible results.
 * Returns CMP_EQUAL, CMP_DIFFER, or CMP_ERROR. */
int compare_inputs(const char* prog, const char* name1, const char* name2,
                   const CmpOptions* opts) {
    bool const file1_is_stdin = (strcmp(name1, "-") == 0);
    bool const file2_is_stdin = (strcmp(name2, "-") == 0);

    /* A single stream cannot serve as both inputs: the first read would
     * consume the data the second stream needed. */
    if (file1_is_stdin && file2_is_stdin) {
        (void)cmd_error(prog, "standard input cannot be used for both files");
        return CMP_ERROR;
    }

    ByteStream s1;
    ByteStream s2;

    s1.fp = open_side(name1, &s1.owns_fp);
    if (s1.fp == nullptr) {
        (void)cmd_perror(prog, name1);
        return CMP_ERROR;
    }

    s2.fp = open_side(name2, &s2.owns_fp);
    if (s2.fp == nullptr) {
        (void)cmd_perror(prog, name2);
        s1.close();
        return CMP_ERROR;
    }

    if (opts->ignore_initial1 > 0) {
        (void)s1.skip(opts->ignore_initial1);
    }
    if (opts->ignore_initial2 > 0) {
        (void)s2.skip(opts->ignore_initial2);
    }

    LineCounter line_counter;
    int64_t byte_number = 0;   /* position relative to the first compared byte */
    int64_t compared = 0;
    DiffReport report;
    int eof_side = 0;          /* 1 => stream 1 ended first, 2 => stream 2 */
    int64_t eof_consumed = 0;  /* bytes read from the short side before EOF */
    int64_t eof_line = 1;      /* line holding the short side's last byte */
    int eof_last_byte = '\n';  /* short side's last byte, for the "in line" test */
    bool const list_diffs = (opts->verbose != 0 && opts->silent == 0);

    for (;;) {
        if (opts->limit >= 0 && compared >= opts->limit) {
            break;
        }

        /* Snapshot the line before reading so an EOF can be attributed to the
         * line that held the short side's final byte. */
        int64_t const line_before = line_counter.line;

        int const a = s1.next();
        int const b = s2.next();

        if (a < 0 && b < 0) {
            break;
        }

        byte_number++;
        compared++;

        if (a < 0 || b < 0) {
            /* One input is a prefix of the other. Remember which one ran out
             * and how far it got; the boundary itself is never a difference. */
            eof_side = (a < 0) ? 1 : 2;
            eof_consumed = (a < 0) ? s1.consumed : s2.consumed;
            eof_line = line_before;
            eof_last_byte = (a < 0) ? s1.last_byte : s2.last_byte;
            break;
        }

        if (a != b) {
            /* Both streams advance in lockstep, so the differing byte sits on
             * the line following the most recent newline. */
            report.note(byte_number, line_counter.line, a, b,
                        list_diffs, opts->print_bytes != 0);
        }
        line_counter.observe(a);
    }

    /* In default/-b mode GNU cmp prints only the first difference. An EOF note
     * is printed only when no difference preceded the boundary, so the two
     * kinds of report never describe the same position. */
    if (opts->silent == 0 && opts->verbose == 0 && report.differ) {
        print_first_difference(name1, name2, report.first_byte, report.first_line,
                               report.first_a, report.first_b, opts->print_bytes != 0);
    }

    /* -l lists every difference, so an early EOF still gets a note naming the
     * short input. */
    if (opts->silent == 0 && eof_side != 0 &&
        (opts->verbose != 0 || !report.differ)) {
        print_eof_diagnostic(prog, (eof_side == 1) ? name1 : name2,
                             eof_consumed, eof_line, eof_last_byte,
                             opts->verbose != 0);
    }

    s1.close();
    s2.close();

    /* Any length difference means the inputs differ, even when the shorter one
     * is a prefix of the longer so that no byte ever mismatched. */
    return (report.differ || eof_side != 0) ? CMP_DIFFER : CMP_EQUAL;
}

}  // namespace

/* ── Main command ───────────────────────────────────────────────────────── */

int cmp_command(int argc, char** argv) {
    const char* prog = (argc > 0 && argv[0] != nullptr) ? argv[0] : "cmp";

    struct arg_lit* opt_print_bytes = arg_lit0("b", "print-bytes", "print differing bytes in octal and mnemonic");
    struct arg_str* opt_ignore_initial = arg_str0("i", "ignore-initial", "SKIP1:SKIP2", "skip the first SKIP bytes of both inputs");
    struct arg_lit* opt_verbose = arg_lit0("l", "verbose", "output byte numbers and differing values");
    struct arg_str* opt_bytes = arg_str0("n", "bytes", "LIMIT", "compare at most LIMIT bytes");
    struct arg_lit* opt_silent = arg_lit0("s", "quiet", "suppress all normal output");
    struct arg_lit* opt_silent_long = arg_lit0(nullptr, "silent", "suppress all normal output");
    struct arg_lit* opt_help = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* opt_version = arg_lit0("V", "version", "output version information and exit");
    struct arg_file* operands = arg_filen(nullptr, nullptr, "FILE", 0, 4, "files to compare, stdin if '-' or omitted");
    struct arg_end* end = arg_end(20);

    ArgTable table({
        opt_print_bytes, opt_ignore_initial, opt_verbose, opt_bytes,
        opt_silent, opt_silent_long, opt_help, opt_version,
        operands, end
    });

    int const nerrors = table.parse(argc, argv);

    if (opt_help->count > 0) {
        print_help(prog);
        return 0;
    }
    if (opt_version->count > 0) {
        print_version("cmp");
        return 0;
    }
    if (nerrors > 0) {
        (void)print_arg_errors(end, prog);
        return CMP_ERROR;
    }

    CmpOptions opts;
    opts.silent = (opt_silent->count > 0 || opt_silent_long->count > 0) ? 1 : 0;
    opts.verbose = opt_verbose->count > 0 ? 1 : 0;
    opts.print_bytes = opt_print_bytes->count > 0 ? 1 : 0;

    /* -l and -s are mutually exclusive, exactly as in GNU cmp. GNU diffutils
     * prefixes its "Try ... --help" hint with the program name, unlike the
     * coreutils commands elsewhere in modbox. */
    if (opts.verbose != 0 && opts.silent != 0) {
        (void)cmd_error(prog, "options -l and -s are incompatible");
        (void)fprintf(stderr, "%s: Try '%s --help' for more information.\n", prog, prog);
        return CMP_ERROR;
    }

    if (opt_bytes->count > 0) {
        int64_t const limit = parse_count(opt_bytes->sval[0]);
        if (limit < 0) {
            (void)cmd_error(prog, "invalid --bytes value '%s'", opt_bytes->sval[0]);
            return CMP_ERROR;
        }
        opts.limit = limit;
    }

    if (opt_ignore_initial->count > 0) {
        if (parse_ignore_initial(opt_ignore_initial->sval[0], &opts) != 0) {
            (void)cmd_error(prog, "invalid --ignore-initial value '%s'",
                            opt_ignore_initial->sval[0]);
            return CMP_ERROR;
        }
    }

    /* Operands: FILE1 [FILE2 [SKIP1 [SKIP2]]]. Both files default to stdin,
     * which compare_inputs rejects as an error. */
    const char* name1 = "-";
    const char* name2 = "-";
    if (operands->count >= 1) { name1 = operands->filename[0]; }
    if (operands->count >= 2) { name2 = operands->filename[1]; }

    /* Positional SKIP1/SKIP2 override --ignore-initial. */
    if (operands->count >= 3) {
        int64_t const skip1 = parse_count(operands->filename[2]);
        if (skip1 < 0) {
            (void)cmd_error(prog, "invalid skip value '%s'", operands->filename[2]);
            return CMP_ERROR;
        }
        opts.ignore_initial1 = skip1;
        if (operands->count < 4) {
            opts.ignore_initial2 = skip1;
        }
    }
    if (operands->count >= 4) {
        int64_t const skip2 = parse_count(operands->filename[3]);
        if (skip2 < 0) {
            (void)cmd_error(prog, "invalid skip value '%s'", operands->filename[3]);
            return CMP_ERROR;
        }
        opts.ignore_initial2 = skip2;
    }

    return compare_inputs(prog, name1, name2, &opts);
}

REGISTER_COMMAND("cmp", cmp_command, "Compare two files byte by byte");
