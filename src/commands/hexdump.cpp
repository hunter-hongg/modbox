// hexdump — display file contents in hex, decimal, octal or ASCII.
//
// A modbox-native implementation of the util-linux `hexdump` utility. The
// behaviour of every supported mode is byte-for-byte identical to the
// reference implementation, so it can be verified by differential testing.
//
// Scope: the fixed display formats (-b -X -c -C -d -o -x and the default),
// data selection (-n, -s), squeezing (-v) and multi-file streaming. The
// format-string engine (-e/-f) and -L/--color are not implemented; see the
// man page.
//
// Zero new dependencies: POSIX plus the C++ STL.

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <argtable3.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/hexdump.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

/* ── Exit status ────────────────────────────────────────────────────────────
 * The reference collapses every error into 1. modbox follows the repo-wide
 * convention of separating usage errors (2) from runtime errors (1), as in
 * `cmp` and `tcpdump`. Recorded as a deviation in the man page. */
constexpr int HEXDUMP_OK = 0;
/* The reference exits 1 for every error, including a usage error, so both
 * failure paths share one status. */
constexpr int HEXDUMP_ERR = 1;

/* One input line holds 16 bytes. The two-byte formats print eight units. */
constexpr size_t BYTES_PER_LINE = 16;

/* The -c data field is padded to this many characters after the offset, so a
 * short final line keeps the column the full-width lines end in. */
constexpr size_t kChar1LineWidth = 64;

/* ── Format description ──────────────────────────────────────────────────── */

enum class Format {
    Default,   /* two-byte hex,  one space between units   */
    Hex2,      /* -x   two-byte hex,  four spaces           */
    Oct2,      /* -o   two-byte octal                      */
    Dec2,      /* -d   two-byte decimal                    */
    Oct1,      /* -b   one-byte octal                      */
    Char1,     /* -c   one-byte character                  */
    Hex1,      /* -X   one-byte hex                        */
    Canonical, /* -C   one-byte hex + |%_p| gutter          */
};

struct Layout {
    size_t unit_bytes;  /* bytes consumed per printed unit */
    int field;          /* width of the rendered field    */
    int sep;            /* spaces between units           */
    int offset_digits;  /* hex digits in the leading offset */
};

/* The exact column geometry of each format, derived from the reference by
 * differential probing (see specs/hexdump_spec.md).
 *
 * A data line is laid out as:
 *
 *     <offset><sep><unit0><sep><unit1>...<sep><blank units...>
 *
 * where the "blank units" pad a short final line out to the full width. The
 * one-byte formats use a 7-digit offset; -C uses 8 digits. */
constexpr Layout kLayouts[] = {
    /* Default  */ {2, 4, 1, 7},
    /* Hex2 -x  */ {2, 4, 4, 7},
    /* Oct2 -o  */ {2, 6, 2, 7},
    /* Dec2 -d  */ {2, 5, 3, 7},
    /* Oct1 -b  */ {1, 3, 1, 7},
    /* Char1 -c */ {1, 4, 0, 7},
    /* Hex1 -X  */ {1, 2, 2, 7},
    /* Canon -C */ {1, 2, 2, 8},
};

Layout layout_of(Format f) { return kLayouts[static_cast<int>(f)]; }

/* The hex digit count of the leading offset field for a format. */
int offset_digits_for(Format f) { return layout_of(f).offset_digits; }

/* ── Byte rendering ──────────────────────────────────────────────────────── */

bool is_printable(uint8_t c) { return c >= 0x20 && c <= 0x7e; }

/* The escape spellings %_c uses. A nullptr entry falls back to three-digit
 * zero-padded octal, which is how NUL and the unnamed controls render. */
const char* const kControlEscape[32] = {
    "\\0", "\\a", "\\b", nullptr, nullptr, "\\t", "\\n", "\\v",
    "\\f", "\\r", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

const char* const kControlName[32] = {
    "nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
    "bs",  "ht",  "lf",  "vt",  "ff",  "cr",  "so",  "si",
    "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
    "can", "em",  "sub", "esc", "fs",  "gs",  "rs",  "us"};

/* %_c: printable characters pass through, known controls use their two
 * character escape, and everything else becomes three-digit octal. */
std::string to_escape_char(uint8_t c) {
    if (is_printable(c)) { return std::string(1, static_cast<char>(c)); }
    if (c < 0x20 && kControlEscape[c] != nullptr) {
        return std::string(kControlEscape[c]);
    }
    char buf[8];
    (void)snprintf(buf, sizeof(buf), "\\%03o", c);
    return std::string(buf);
}

/* %_p: printable, else a single '.'. */
std::string to_printable(uint8_t c) {
    return is_printable(c) ? std::string(1, static_cast<char>(c)) : std::string(".");
}

/* %_u: US ASCII with lower-case control names; bytes above 0x7e render as a
 * bare hex string. */
std::string to_ascii_name(uint8_t c) {
    if (c == 0x7f) { return "del"; }
    if (c < 0x20) { return kControlName[c]; }
    if (c <= 0x7e) { return std::string(1, static_cast<char>(c)); }
    char buf[8];
    (void)snprintf(buf, sizeof(buf), "%02x", c);
    return std::string(buf);
}

/* The -c format escapes a non-printable byte. A zero byte and the handful of
 * control characters with a backslash form use the two column short escape;
 * every other control byte is shown as a three digit octal number, which with
 * its leading space fills the same four column cell. */
std::string to_char1(uint8_t c) {
    if (is_printable(c)) { return std::string(1, static_cast<char>(c)); }
    char buf[8];
    switch (c) {
        case 0:    (void)snprintf(buf, sizeof(buf), "\\0"); break;
        case '\a': (void)snprintf(buf, sizeof(buf), "\\a"); break;
        case '\b': (void)snprintf(buf, sizeof(buf), "\\b"); break;
        case '\f': (void)snprintf(buf, sizeof(buf), "\\f"); break;
        case '\n': (void)snprintf(buf, sizeof(buf), "\\n"); break;
        case '\r': (void)snprintf(buf, sizeof(buf), "\\r"); break;
        case '\t': (void)snprintf(buf, sizeof(buf), "\\t"); break;
        case '\v': (void)snprintf(buf, sizeof(buf), "\\v"); break;
        default:   (void)snprintf(buf, sizeof(buf), " %03o", c); break;
    }
    return std::string(buf);
}

/* Render one data unit. `pad` selects the blank form used to pad a short
 * final line out to the full width. */
std::string render_unit(Format f, const uint8_t* p, bool pad) {
    const Layout lay = layout_of(f);
    char buf[32];
    if (pad) {
        /* A padded unit is a run of blanks the width of the field. */
        std::memset(buf, ' ', static_cast<size_t>(lay.field));
        buf[lay.field] = '\0';
        return std::string(buf);
    }
    switch (f) {
        case Format::Default:
        case Format::Hex2: {
            uint16_t v;
            std::memcpy(&v, p, 2);
            (void)snprintf(buf, sizeof(buf), "%0*x", lay.field, v);
            break;
        }
        case Format::Oct2: {
            uint16_t v;
            std::memcpy(&v, p, 2);
            (void)snprintf(buf, sizeof(buf), "%0*o", lay.field, v);
            break;
        }
        case Format::Dec2: {
            uint16_t v;
            std::memcpy(&v, p, 2);
            (void)snprintf(buf, sizeof(buf), "%0*u", lay.field, v);
            break;
        }
        case Format::Oct1:
            (void)snprintf(buf, sizeof(buf), "%0*o", lay.field, p[0]);
            break;
        case Format::Hex1:
            (void)snprintf(buf, sizeof(buf), "%0*x", lay.field, p[0]);
            break;
        case Format::Char1:
            return pad ? std::string(static_cast<size_t>(lay.field), ' ')
                        : to_escape_char(p[0]);
        case Format::Canonical:
            (void)snprintf(buf, sizeof(buf), "%0*x", lay.field, p[0]);
            break;
    }
    if (pad) {
        return std::string(static_cast<size_t>(lay.field), ' ');
    }
    return std::string(buf);
}

/* Read a two-byte unit in the host's native byte order, matching the
 * reference on the little-endian platforms modbox targets. */
uint16_t native_u16(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

/* ── Line emitter with duplicate squeezing ────────────────────────────────── */

class Emitter {
public:
    Emitter(FILE* out, bool squeeze, size_t slots)
        : out_(out), squeeze_(squeeze), state_(slots) {}

    /* A data line. Two consecutive lines of the same format are "identical"
     * when their bodies match, the offset being deliberately excluded, so the
     * squeeze state is kept per format. A repeated line prints nothing: the
     * block driver writes a single * for the whole block. */
    bool data_line(size_t slot, const std::string& offset,
                   const std::string& body) {
        Squeeze& q = state_[slot];
        if (squeeze_ && q.have_prev && body == q.prev) { return true; }
        write_line(offset + body);
        q.prev = body;
        q.have_prev = true;
        star_shown_ = false;
        return false;
    }

    /* The single * that stands in for a run of repeated blocks. It is written
     * once per run, so `star_shown` has to be cleared by a block that prints
     * at least one line. */
    void star() {
        if (star_shown_) { return; }
        write_line("*");
        star_shown_ = true;
    }

    /* The closing offset line, which never takes part in squeezing. */
    void final_offset(const std::string& offset) {
        write_line(offset);
        for (Squeeze& q : state_) { q.have_prev = false; }
        star_shown_ = false;
    }

    bool failed() const { return failed_; }

private:
    void write_line(const std::string& s) {
        if (failed_) { return; }
        if (std::fwrite(s.data(), 1, s.size(), out_) != s.size()) {
            failed_ = true;
            return;
        }
        if (std::fputc('\n', out_) == EOF) { failed_ = true; }
    }

    struct Squeeze {
        std::string prev;
        bool have_prev = false;
    };

    FILE* out_;
    bool squeeze_;
    std::vector<Squeeze> state_;
    bool star_shown_ = false;
    bool failed_ = false;
};

/* ── Fixed-format renderer ────────────────────────────────────────────────── */

std::string render_line(Format fmt, const std::vector<uint8_t>& data,
                        size_t pos, uint64_t base_offset,
                        std::string& line_offset) {
    const Layout lay = layout_of(fmt);
    const size_t step = lay.unit_bytes;
    const size_t per_line = BYTES_PER_LINE / step;

    /* The canonical format closes each line with a |...| gutter whose width
     * follows the number of real bytes on the line. */
    const bool canonical = (fmt == Format::Canonical);

    {
        const size_t avail = data.size() - pos;
        /* A two-byte format completes the last unit with a zero pad when an
         * odd number of bytes remains, so round up to whole units. */
        const size_t shown =
            (avail < per_line * step) ? ((avail + step - 1) / step) * step
                                      : per_line * step;
        const size_t real_bytes = (avail < shown) ? avail : shown;

        char offbuf[32];
        (void)snprintf(offbuf, sizeof(offbuf), "%0*llx", lay.offset_digits,
                       static_cast<unsigned long long>(base_offset + pos));

        std::string body;
        if (canonical) {
            /* The canonical layout groups the hex field in eights and puts a
             * |...| gutter straight after it: "  hh hh ... hh  |text|". */
            body += "  ";
            for (size_t g = 0; g < 2; ++g) {
                for (size_t i = 0; i < 8; ++i) {
                    const size_t idx = g * 8 + i;
                    if (i > 0) { body += ' '; }
                    if (idx < real_bytes) {
                        char b[8];
                        (void)snprintf(b, sizeof(b), "%02x", data[pos + idx]);
                        body += b;
                    } else {
                        body += "  ";
                    }
                }
                body += "  ";
            }
            body += '|';
            /* The gutter is trimmed to the bytes actually on this line, while
             * the hex field above stays full width. */
            for (size_t i = 0; i < real_bytes; ++i) {
                body += to_printable(data[pos + i]);
            }
            body += '|';
        } else if (fmt == Format::Char1) {
            /* The -c format prints only the units that hold data and pads the
             * line out to a fixed width, rather than emitting blank units. A
             * byte is right aligned in a three column cell; an escape that is
             * longer simply widens its own cell. */
            const size_t units = (real_bytes + step - 1) / step;
            for (size_t u = 0; u < units; ++u) {
                const size_t at = u * step;
                /* Every cell is four columns wide: a bare character is right
                 * aligned in it, and an escape fills it exactly. */
                const std::string cell = to_char1(data[pos + at]);
                if (cell.size() < 4) { body.append(4 - cell.size(), ' '); }
                body += cell;
            }
            /* A short line is padded out to the width a full line reaches. */
            if (body.size() < kChar1LineWidth) {
                body.append(kChar1LineWidth - body.size(), ' ');
            }
        } else {
            body.append(static_cast<size_t>(lay.sep), ' ');
            for (size_t u = 0; u < per_line; ++u) {
                if (u > 0) { body.append(static_cast<size_t>(lay.sep), ' '); }
                const size_t at = u * step;
                if (at < real_bytes) {
                    /* A two-byte unit may be truncated by the end of the
                     * stream; pad the missing byte with zero so the reader
                     * never runs past the buffer. */
                    uint8_t unit[2] = {0, 0};
                    const size_t left = (at < avail) ? (avail - at) : 0;
                    const size_t have = (left < step) ? left : step;
                    for (size_t j = 0; j < have; ++j) { unit[j] = data[pos + at + j]; }
                    body += render_unit(fmt, unit, false);
                } else {
                    body += render_unit(fmt, nullptr, true);
                }
            }
        }

        line_offset = std::string(offbuf);
        return body;
    }
}

/* The formats interleave: every data block is printed once per requested
 * format, in the order the options appear, and only then does the next block
 * begin. A block is squeezed as a whole, so with two formats the bodies of
 * both lines must match before a star replaces them. */
void run_formats(Emitter& em, const std::vector<Format>& formats,
                 const std::vector<uint8_t>& data, uint64_t base_offset) {
    /* Every format has its own unit size, so the block boundaries are those
     * of the smallest unit requested; a wider format then covers several
     * blocks and simply prints fewer lines. Advancing by the smallest step
     * keeps the interleaving aligned across formats. */
    size_t step = BYTES_PER_LINE;
    for (const Format f : formats) {
        const size_t s = layout_of(f).unit_bytes;
        if (s < step) { step = s; }
    }
    const size_t per_line = BYTES_PER_LINE / step;

    for (size_t pos = 0; pos < data.size(); pos += per_line * step) {
        bool repeated = false;
        for (size_t k = 0; k < formats.size(); ++k) {
            std::string offset;
            const std::string body = render_line(formats[k], data, pos,
                                                  base_offset, offset);
            if (em.data_line(k, offset, body)) { repeated = true; }
        }
        /* Every format of a repeated block prints nothing, so one * stands in
         * for the whole block. A run of repeats still yields a single *. */
        if (repeated) { em.star(); }
    }
}

/* ── Option parsing helpers ──────────────────────────────────────────────── */

/* Parse a length/offset argument. Accepts a plain decimal number, a 0x
 * prefixed hexadecimal number, and the multiplicative suffixes KiB/MiB/... and
 * KB/MB/... that the reference accepts. */
bool parse_size(const char* s, uint64_t* out) {
    if (s == nullptr || *s == '\0') { return false; }
    char* end = nullptr;
    errno = 0;
    const unsigned long long v = std::strtoull(s, &end, 0);
    if (errno != 0 || end == s) { return false; }

    uint64_t mult = 1;
    if (*end != '\0') {
        const char c = *end;
        switch (c) {
            case 'i': case 'I':
                mult = 1024ULL;
                ++end;
                if (*end == 'b' || *end == 'B') { ++end; }
                break;
            case 'k': case 'K': mult = 1024ULL << 10; ++end; break;
            case 'm': case 'M': mult = 1024ULL << 20; ++end; break;
            case 'g': case 'G': mult = 1024ULL << 30; ++end; break;
            case 't': case 'T': mult = 1024ULL << 40; ++end; break;
            case 'p': case 'P': mult = 1024ULL << 50; ++end; break;
            case 'e': case 'E': mult = 1024ULL << 60; ++end; break;
            default: return false;
        }
        if (*end == 'b' || *end == 'B') { ++end; }
        if (*end != '\0') { return false; }
    }
    *out = static_cast<uint64_t>(v) * mult;
    return true;
}

const char* kUsage =
    "Usage: hexdump [options] file...\n"
    "Display file contents in hexadecimal, decimal, octal, or ASCII.\n"
    "\n"
    "Options:\n"
    "  -b, --one-byte-octal   one-byte octal display\n"
    "  -X, --one-byte-hex     one-byte hexadecimal display\n"
    "  -c, --one-byte-char    one-byte character display\n"
    "  -C, --canonical        canonical hex+ASCII display\n"
    "  -d, --two-bytes-decimal  two-byte decimal display\n"
    "  -o, --two-bytes-octal  two-byte octal display\n"
    "  -x, --two-bytes-hex    two-byte hexadecimal display\n"
    "  -n, --length LEN       interpret only LEN bytes of input\n"
    "  -s, --skip OFFSET      skip OFFSET bytes from the beginning of input\n"
    "  -v, --no-squeezing     display all input data (no duplicate suppression)\n"
    "  -h, --help             display this help and exit\n"
    "  -V, --version          output version information and exit\n";

}  // namespace

int hexdump_command(int argc, char** argv) {
    struct arg_lit *help = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit *vers = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit *b_opt = arg_lit0("b", "one-byte-octal", nullptr);
    struct arg_lit *X_opt = arg_lit0("X", "one-byte-hex", nullptr);
    struct arg_lit *c_opt = arg_lit0("c", "one-byte-char", nullptr);
    struct arg_lit *C_opt = arg_lit0("C", "canonical", nullptr);
    struct arg_lit *d_opt = arg_lit0("d", "two-bytes-decimal", nullptr);
    struct arg_lit *o_opt = arg_lit0("o", "two-bytes-octal", nullptr);
    struct arg_lit *x_opt = arg_lit0("x", "two-bytes-hex", nullptr);
    struct arg_lit *v_opt = arg_lit0("v", "no-squeezing", nullptr);
    struct arg_str *n_opt =
        arg_str0("n", "length", "LEN", "interpret only LEN bytes of input");
    struct arg_str *s_opt = arg_str0("s", "skip", "OFFSET",
                                     "skip OFFSET bytes from the beginning of input");
    struct arg_file *files = arg_filen(nullptr, nullptr, "FILE...", 0, 1000,
                                        "input files ('-' for standard input)");
    struct arg_end *end = arg_end(20);

    ArgTable table{help, vers, b_opt, X_opt, c_opt, C_opt, d_opt, o_opt, x_opt,
                   v_opt, n_opt, s_opt, files, end};
    if (table.parse(argc, argv) != 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help->count > 0) {
        (void)fputs(kUsage, stdout);
        return HEXDUMP_OK;
    }
    if (vers->count > 0) {
        print_version(argv[0]);
        return HEXDUMP_OK;
    }

    /* Each format option requests its own pass over the whole stream, in
     * the order the options appear on the command line. With none given the
     * default two-byte hexadecimal format is used. */
    std::vector<Format> formats;
    for (int i = 0; i < argc; ++i) {
        const char* a = argv[i];
        if (a[0] != '-' || a[1] == '\0') { continue; }
        const char* o = a + 1;
        const char* eq = std::strchr(o, '=');
        const size_t olen = (eq != nullptr) ? static_cast<size_t>(eq - o)
                                            : std::strlen(o);
        for (size_t k = 0; k < olen; ++k) {
            switch (o[k]) {
                case 'b': formats.push_back(Format::Oct1); break;
                case 'X': formats.push_back(Format::Hex1); break;
                case 'c': formats.push_back(Format::Char1); break;
                case 'C': formats.push_back(Format::Canonical); break;
                case 'd': formats.push_back(Format::Dec2); break;
                case 'o': formats.push_back(Format::Oct2); break;
                case 'x': formats.push_back(Format::Hex2); break;
                default: break;
            }
        }
    }
    if (formats.empty()) { formats.push_back(Format::Default); }

    uint64_t skip = 0;
    if (s_opt->count > 0 && !parse_size(s_opt->sval[0], &skip)) {
        (void)fprintf(stderr, "%s: invalid skip value '%s'\n", argv[0], s_opt->sval[0]);
        return HEXDUMP_ERR;
    }
    uint64_t length = UINT64_MAX;
    if (n_opt->count > 0 && !parse_size(n_opt->sval[0], &length)) {
        (void)fprintf(stderr, "%s: invalid length value '%s'\n", argv[0], n_opt->sval[0]);
        return HEXDUMP_ERR;
    }

    Emitter em(stdout, v_opt->count == 0, formats.size());
    int rc = HEXDUMP_OK;

    /* All input files form one continuous stream: they are concatenated
     * before dumping, so the offsets run on across file boundaries. -s skips
     * into that concatenation and -n bounds the total number of bytes. */
    std::vector<uint8_t> all;
    uint64_t skipped = 0;   /* bytes discarded by -s so far */
    uint64_t dumped = 0;    /* bytes kept by -n so far */
    uint64_t stream = 0;    /* total bytes read from every file   */
    bool done = false;

    /* With no file arguments the stream is standard input. */
    const int file_count = (files->count > 0) ? files->count : 1;
    for (int i = 0; i < file_count && !done; ++i) {
        FILE* in = stdin;
        const char* name = "-";
        const bool close_in =
            (files->count > 0) && (std::strcmp(files->filename[i], "-") != 0);
        if (close_in) {
            name = files->filename[i];
            in = std::fopen(files->filename[i], "rb");
            if (in == nullptr) {
                (void)fprintf(stderr, "%s: %s: %s\n", argv[0], name,
                              std::strerror(errno));
                rc = HEXDUMP_ERR;
                continue;
            }
        }

        uint8_t buf[65536];
        for (;;) {
            if (length != UINT64_MAX && dumped >= length) { done = true; break; }
            const size_t got = std::fread(buf, 1, sizeof(buf), in);
            if (got == 0) { break; }
            stream += got;
            for (size_t b = 0; b < got; ++b) {
                if (skipped < skip) { ++skipped; continue; }
                if (length != UINT64_MAX && dumped >= length) { done = true; break; }
                all.push_back(buf[b]);
                ++dumped;
            }
            if (got < sizeof(buf)) { break; }
        }
        if (std::ferror(in) != 0) {
            (void)fprintf(stderr, "%s: %s: %s\n", argv[0], name,
                          std::strerror(errno));
            rc = HEXDUMP_ERR;
        }
        if (close_in) { (void)fclose(in); }
    }

    /* The closing offset line is written once, after all passes. A -n of zero
     * suppresses everything, but a -s that runs past the end of the input
     * still reports the resulting stream length. */
    if (!all.empty()) {
        run_formats(em, formats, all, skip);
        char finalbuf[32];
        (void)snprintf(finalbuf, sizeof(finalbuf), "%0*llx",
                       offset_digits_for(formats.back()),
                       static_cast<unsigned long long>(skip + all.size()));
        em.final_offset(std::string(finalbuf));
    } else if (stream > 0) {
        /* Something was read but nothing survived the -s, so there are no data
         * lines: the closing offset alone reports where the stream ended. An
         * input that held no bytes at all stays completely silent. */
        char finalbuf[32];
        (void)snprintf(finalbuf, sizeof(finalbuf), "%0*llx",
                       offset_digits_for(formats.back()),
                       static_cast<unsigned long long>(stream));
        em.final_offset(std::string(finalbuf));
    }
    if (em.failed()) { rc = HEXDUMP_ERR; }

    return rc;
}

REGISTER_COMMAND("hexdump", hexdump_command,
                 "Display file contents in hexadecimal, decimal, octal, or ASCII")
