// xxd — make a hexdump or do the reverse.
//
// A modbox-native implementation of the classic `xxd` utility (vim-common).
// Behaviour is intentionally byte-for-byte compatible with the reference tool
// for every supported mode, so it can be verified by differential testing.
//
// Supported options: -a -b -C -c -d -E -e -g -h -i -l -n -o -p -ps -r -R -s
// -t -u -v.

#include <cctype>
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

#include "commands/xxd.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

/* Exit status, matching the reference xxd. */
constexpr int XXD_OK = 0;
constexpr int XXD_USAGE = 1;   /* bad option / incompatible combination     */
constexpr int XXD_IOERR = 2;   /* cannot open input / write output          */

/* Default line/group widths per mode, as in the reference tool. */
constexpr int DEFAULT_COLS = 16;
constexpr int CINCLUDE_COLS = 12;
constexpr int PLAIN_COLS = 30;
constexpr int DEFAULT_GROUP = 2;
constexpr int E_GROUP = 4;

struct XxdOptions {
    bool autoskip = false;       /* -a */
    bool binary = false;         /* -b */
    bool capitalize = false;     /* -C */
    int cols = 0;                /* -c; 0 = mode default */
    bool decimal_offsets = false;/* -d */
    bool ebcdic = false;         /* -E */
    bool little_endian = false;  /* -e */
    int group = 0;               /* -g; 0 = mode default (0 also means one column) */
    bool group_set = false;      /* whether -g was given (distinguish 0 from unset) */
    bool c_include = false;      /* -i */
    long length = -1;            /* -l; -1 = unlimited */
    std::string var_name;        /* -n */
    long offset_bias = 0;        /* -o */
    long seek = 0;               /* -s */
    int seek_sign = 0;           /* -1 = backwards, +1 = relative, 0 = absolute */
    bool plain = false;          /* -ps */
    bool reverse = false;        /* -r */
    bool terminate = false;      /* -t */
    bool upper = false;          /* -u */
    int color = -1;              /* -R: -1 unset/auto, 0 never, 1 always */
};

/* ── Small helpers ─────────────────────────────────────────────────────── */

bool is_tty_stdout() {
    return isatty(STDOUT_FILENO) != 0;
}

/* Derive a C identifier from a path by replacing every non-alphanumeric,
 * non-underscore character with '_'. Mirrors xxd's name mangling. */
std::string derive_var_name(const char* path) {
    std::string base;
    if (path == nullptr || strcmp(path, "-") == 0) {
        base = "stdin";
    } else {
        const char* slash = strrchr(path, '/');
        base = (slash != nullptr) ? (slash + 1) : path;
    }
    for (char& c : base) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_') {
            c = '_';
        }
    }
    if (base.empty()) { base = "stdin"; }
    return base;
}

/* Read the whole input into memory (xxd streams, but the reference's layout
 * only needs bounded lookahead; buffering whole input keeps the code simple).
 * Returns 0 on success. */
int read_all(FILE* f, std::vector<uint8_t>& out) {
    uint8_t buf[65536];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return ferror(f) != 0 ? -1 : 0;
}

/* ── ASCII / EBCDIC gutter ─────────────────────────────────────────────── */

/* EBCDIC-to-ASCII transliteration for the printable gutter (-E). This is the
 * table the reference xxd uses (IBM code page 037); every byte that has no
 * printable ASCII equivalent renders as '.'. */
char ebcdic_to_ascii(uint8_t c) {
    static const char table[256] = {
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.',
        ' ', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '<', '(', '+', '|',
        '&', '.', '.', '.', '.', '.', '.', '.', '.', '.', '!', '$', '*', ')', ';', '~',
        '-', '/', '.', '.', '.', '.', '.', '.', '.', '.', '.', ',', '%', '_', '>', '?',
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '`', ':', '#', '@', '\'', '=', '"',
        '.', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', '.', '.', '.', '.', '.', '.',
        '.', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', '^', '.', '.', '.', '.', '.',
        '.', '.', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '.', '.', '.', '[', '.', '.',
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', ']', '.', '.',
        '{', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', '.', '.', '.', '.', '.', '.',
        '}', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', '.', '.', '.', '.', '.', '.',
        '\\', '.', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '.', '.', '.', '.', '.', '.',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '.', '.', '.', '.', '.',
    };
    return table[c];
}

char gutter_char(uint8_t c, bool ebcdic) {
    uint8_t v = ebcdic ? static_cast<uint8_t>(ebcdic_to_ascii(c)) : c;
    if (v >= 0x20 && v < 0x7f) { return static_cast<char>(v); }
    return '.';
}

/* ── Hex byte formatting ───────────────────────────────────────────────── */

char hex_digit(int nibble, bool upper) {
    static const char lower[] = "0123456789abcdef";
    static const char upperc[] = "0123456789ABCDEF";
    return (upper ? upperc : lower)[nibble & 0xf];
}

/* ── Dump mode ─────────────────────────────────────────────────────────── */

struct DumpConfig {
    int cols;
    int group;       /* 0 means one unbroken column */
    bool binary;
    bool little_endian;
    bool upper;
    bool ebcdic;
    bool autoskip;
    bool decimal_offsets;
    bool color;      /* -R always (auto/never are no-ops on a pipe) */
};

/* Print the address field: always 8 digits, hex or decimal, followed by ": ". */
void print_offset(uint64_t addr, const DumpConfig& cfg) {
    if (cfg.decimal_offsets) {
        printf("%08llu: ", static_cast<unsigned long long>(addr));
    } else {
        printf("%08llx: ", static_cast<unsigned long long>(addr));
    }
}

/* Effective group size: xxd treats -g0 as "one group covering the whole line"
 * (i.e. group == cols), and -b as fixed bit groups. */
int effective_group(const DumpConfig& cfg) {
    if (cfg.binary) { return 1; }
    if (cfg.group == 0) { return cfg.cols; }
    return cfg.group;
}

/* Width of the final, possibly partial, group on a full line. A complete group
 * is always "2*g hex digits + 1 space". A partial group still contributes its
 * trailing space; little-endian (-e) additionally right-aligns its real bytes
 * inside the full 2*g-wide cell, because missing bytes sit at the high end. */
int partial_group_width(const DumpConfig& cfg, int g, int rem) {
    if (cfg.little_endian) { return (2 * (g + 1)) - 1; }
    return (2 * (rem + 1)) - 1;
}

/* Character width of the hex field for a line filled to `cols` bytes,
 * including the single trailing space each group contributes, plus the one
 * space that separates the hex field from the printable gutter. Every line is
 * padded to this width so the gutters line up, whether or not the line is
 * full. */
int hex_field_width(const DumpConfig& cfg) {
    if (cfg.binary) {
        /* cols bytes, each "01010101 " (8 bits + space), then one space */
        return (cfg.cols * 9) + 1;
    }
    int const g = effective_group(cfg);
    int const full_groups = cfg.cols / g;
    int const rem = cfg.cols % g;
    int width = full_groups * ((2 * g) + 1);
    if (rem != 0) { width += partial_group_width(cfg, g, rem); }
    return width + 1;
}

/* Render one line of `len` bytes starting at data, at file position `addr`.
 * Layout: 8-digit offset + ": ", then whole-group hex pairs each followed by a
 * single space, padded out to the full-line width plus two more spaces, then
 * the printable gutter. This matches the reference byte for byte. */
/* ── -R colorization ───────────────────────────────────────────────────── */

/* Each byte gets one of a few colours from the reference's palette. The choice
 * only depends on the byte value; adjacent bytes with the same colour are
 * emitted inside a single escape sequence. */
enum class ByteColor { White, Red, Green, Yellow, Blue };

ByteColor byte_color(uint8_t c) {
    if (c == 0) { return ByteColor::White; }          /* nul               */
    if (c == 0xff) { return ByteColor::Blue; }        /* 0xff              */
    if (c == '\t' || c == '\n' || c == '\r') {
        return ByteColor::Yellow;                      /* tab, LF, CR       */
    }
    if (c < 0x20 || c == 0x7f) { return ByteColor::Red; }  /* control       */
    if (c >= 0x80) { return ByteColor::Red; }         /* high bit set      */
    return ByteColor::Green;                           /* printable ASCII   */
}

const char* color_seq(ByteColor c) {
    switch (c) {
        case ByteColor::White:  return "\033[1;37m";
        case ByteColor::Red:    return "\033[1;31m";
        case ByteColor::Green:  return "\033[1;32m";
        case ByteColor::Yellow: return "\033[1;33m";
        case ByteColor::Blue:   return "\033[1;34m";
    }
    return "";
}

/* Emit `text` wrapped in the escape sequence for `c`, then reset. An empty
 * run is skipped so no stray escapes appear for missing data. */
void colorize(const std::string& text, ByteColor c) {
    if (text.empty()) { return; }
    printf("%s%s\033[0m", color_seq(c), text.c_str());
}

/* Render one line of `len` bytes starting at data, at file position `addr`.
 * Layout: 8-digit offset + ": ", then whole-group hex pairs each followed by a
 * single space, padded out to the full-line width plus two more spaces, then
 * the printable gutter. This matches the reference byte for byte. When
 * `cfg.color` is set the hex field, the missing-byte padding and the gutter are
 * wrapped in ANSI colours the way the reference does. */
void dump_line(const uint8_t* data, int len, uint64_t addr, const DumpConfig& cfg) {
    print_offset(addr, cfg);

    /* Each element of `hex_runs` is one maximal same-coloured run in the hex
     * field; `gutter_runs` is the same for the printable gutter. Padding that
     * represents bytes past the end of the line is collected separately: it is
     * drawn in red between the hex field and the gutter. */
    struct Run { std::string text; ByteColor color; bool sep; };
    std::vector<Run> hex_runs;
    std::vector<Run> gutter_runs;
    std::string missing;   /* trailing spaces for absent hex digits        */

    /* `sep` runs are the plain single-space separators between byte groups;
     * they are kept as their own runs so they can be printed uncoloured even
     * when they happen to share a colour with the neighbouring bytes. */
    auto push_hex = [&](const std::string& t, ByteColor c) {
        if (!hex_runs.empty() && !hex_runs.back().sep && hex_runs.back().color == c) {
            hex_runs.back().text += t;
        } else {
            hex_runs.push_back({t, c, false});
        }
    };
    auto push_sep = [&]() { hex_runs.push_back({" ", ByteColor::Red, true}); };

    if (cfg.binary) {
        for (int i = 0; i < len; i++) {
            std::string bits;
            for (int b = 7; b >= 0; b--) {
                bits += ((data[i] >> b) & 1) != 0 ? '1' : '0';
            }
            push_hex(bits, byte_color(data[i]));
            push_sep();
        }
    } else {
        int const g = effective_group(cfg);
        for (int i = 0; i < len; i += g) {
            int const chunk = (len - i < g) ? (len - i) : g;
            if (cfg.little_endian) {
                /* Little-endian: the real bytes sit at the high end of the
                 * group, so missing bytes become *leading* spaces. */
                std::string pad;
                for (int j = 0; j < g - chunk; j++) { pad += "  "; }
                if (!pad.empty()) {
                    push_hex(pad, ByteColor::Red);
                    missing += pad;
                }
                for (int j = 0; j < chunk; j++) {
                    uint8_t const byte = data[i + chunk - 1 - j];
                    std::string t;
                    t += hex_digit(byte >> 4, cfg.upper);
                    t += hex_digit(byte & 0xf, cfg.upper);
                    push_hex(t, byte_color(byte));
                }
            } else {
                for (int j = 0; j < chunk; j++) {
                    std::string t;
                    t += hex_digit(data[i + j] >> 4, cfg.upper);
                    t += hex_digit(data[i + j] & 0xf, cfg.upper);
                    push_hex(t, byte_color(data[i + j]));
                }
            }
            push_sep();
        }
    }

    std::string hex = "";
    for (const Run& r : hex_runs) { hex += r.text; }
    /* Pad the hex field (which already ends with the last group's space) out to
     * the full-line width; the width constant includes the two gutter spaces. */
    int const width = hex_field_width(cfg);
    std::string pad;
    while (static_cast<int>(hex.size() + pad.size()) < width) { pad += ' '; }

    if (!cfg.color) {
        printf("%s%s", hex.c_str(), pad.c_str());
        for (int i = 0; i < len; i++) {
            putchar(gutter_char(data[i], cfg.ebcdic));
        }
        putchar('\n');
        return;
    }

    /* Colorized hex field. The bytes that are actually present are printed in
     * their own colours; the absent ones are shown as a red block of spaces,
     * with a one- or two-space plain margin before the gutter. The widths are
     * derived from the full-line width so the gutters still line up with the
     * uncoloured output. */
    int nseps = 0;
    std::size_t sum_hex = 0;
    for (const Run& r : hex_runs) {
        if (r.sep) { nseps++; continue; }   /* separator, not a data run */
        sum_hex += r.text.size();
    }
    int const fill = width - static_cast<int>(sum_hex) - nseps;
    int const g = effective_group(cfg);
    int const red = cfg.cols - len;
    int const trail = (g > 0 && (len % g) != 0) ? 2 : 1;

    /* Print the byte groups, merging adjacent same-coloured hex digits into a
     * single escape sequence, and re-printing the plain separator spaces
     * between groups. */
    std::string acc;
    ByteColor acc_color = ByteColor::Green;
    auto flush = [&]() {
        if (!acc.empty()) { colorize(acc, acc_color); acc.clear(); }
    };
    for (const Run& r : hex_runs) {
        if (r.sep) {
            /* group separator: always plain, and it also ends the current
             * colour run so escapes reset at every group boundary. */
            flush();
            putchar(' ');
            continue;
        }
        if (!acc.empty() && r.color != acc_color) { flush(); }
        acc += r.text;
        acc_color = r.color;
    }
    flush();

    int plain = 0;
    if (red == 0) {
        plain = fill;
    } else {
        /* The separator after the last data group already supplies one of the
         * padding columns. */
        plain = fill - red - trail;
        if (plain < 0) { plain = 0; }
    }
    for (int i = 0; i < plain; i++) { putchar(' '); }
    if (red > 0) {
        std::string red_block(static_cast<size_t>(red), ' ');
        colorize(red_block, ByteColor::Red);
        for (int i = 0; i < trail; i++) { putchar(' '); }
    }

    /* Gutter: adjacent same-coloured characters share an escape sequence, and
     * the characters past the end of the line are padded in white. */
    std::string gt;
    ByteColor prev = ByteColor::Green;
    bool have = false;
    for (int i = 0; i < len; i++) {
        ByteColor const c = byte_color(data[i]);
        char const ch = gutter_char(data[i], cfg.ebcdic);
        if (have && c != prev && !gt.empty()) {
            colorize(gt, prev);
            gt.clear();
        }
        gt += ch;
        prev = c;
        have = true;
    }
    colorize(gt, prev);
    putchar('\n');
}

bool line_is_zero(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] != 0) { return false; }
    }
    return true;
}

int run_dump(const std::vector<uint8_t>& data, const XxdOptions& opts) {
    DumpConfig cfg;
    cfg.binary = opts.binary;
    cfg.little_endian = opts.little_endian;
    cfg.upper = opts.upper;
    cfg.ebcdic = opts.ebcdic;
    cfg.autoskip = opts.autoskip;
    cfg.decimal_offsets = opts.decimal_offsets;
    cfg.color = opts.color == 1;

    if (opts.cols > 0) {
        cfg.cols = opts.cols;
    } else if (opts.binary) {
        cfg.cols = 6;   /* reference bit-dump default */
    } else {
        cfg.cols = DEFAULT_COLS;
    }
    if (opts.group_set) {
        cfg.group = opts.group;
    } else {
        cfg.group = opts.little_endian ? E_GROUP : DEFAULT_GROUP;
    }

    long const start = opts.seek >= 0 ? opts.seek : 0;
    uint64_t addr = static_cast<uint64_t>(start) + static_cast<uint64_t>(opts.offset_bias);
    size_t pos = static_cast<size_t>(start);
    long remaining = opts.length;

    /* Autoskip bookkeeping. The reference prints the first line of a run of
     * zero lines and, once the run reaches three lines, collapses the rest of
     * the run into a single "*". Runs of one or two zero lines are printed in
     * full, so the decision can only be made once the run ends: hold back the
     * second zero line and flush it if the run stops there. */
    int zero_count = 0;            /* zero lines seen in the current run     */
    size_t zero_pos = 0;           /* the held-back second line              */
    int zero_len = 0;
    uint64_t zero_addr = 0;

    while (pos < data.size()) {
        if (remaining == 0) { break; }
        int chunk = cfg.cols;
        if (opts.length >= 0 && remaining < chunk) {
            chunk = static_cast<int>(remaining);
        }
        if (pos + static_cast<size_t>(chunk) > data.size()) {
            chunk = static_cast<int>(data.size() - pos);
        }
        if (chunk <= 0) { break; }

        bool const zero = cfg.autoskip && line_is_zero(data.data() + pos, chunk);
        if (zero) {
            zero_count++;
            if (zero_count == 1) {
                dump_line(data.data() + pos, chunk, addr, cfg);
            } else if (zero_count == 2) {
                /* Held back: a short run stays hidden until it ends. */
                zero_pos = pos;
                zero_len = chunk;
                zero_addr = addr;
            } else if (zero_count == 3) {
                printf("*\n");
            }
        } else {
            if (zero_count == 2) {
                dump_line(data.data() + zero_pos, zero_len, zero_addr, cfg);
            }
            zero_count = 0;
            dump_line(data.data() + pos, chunk, addr, cfg);
        }

        pos += static_cast<size_t>(chunk);
        addr += static_cast<uint64_t>(chunk);
        if (opts.length >= 0) { remaining -= chunk; }
    }
    /* A zero run that reaches the end of the input never saw a following line;
     * if it was only two lines long, the held-back line still has to appear. */
    if (zero_count == 2) {
        dump_line(data.data() + zero_pos, zero_len, zero_addr, cfg);
    }
    return XXD_OK;
}

/* ── Plain (-ps) mode ──────────────────────────────────────────────────── */

int run_plain(const std::vector<uint8_t>& data, const XxdOptions& opts) {
    int const cols = opts.cols > 0 ? opts.cols : PLAIN_COLS;
    long const start = opts.seek >= 0 ? opts.seek : 0;
    size_t pos = static_cast<size_t>(start);
    long remaining = opts.length;

    while (pos < data.size()) {
        if (remaining == 0) { break; }
        int chunk = cols;
        if (opts.length >= 0 && remaining < chunk) { chunk = static_cast<int>(remaining); }
        if (pos + static_cast<size_t>(chunk) > data.size()) {
            chunk = static_cast<int>(data.size() - pos);
        }
        for (int i = 0; i < chunk; i++) {
            putchar(hex_digit(data[pos + i] >> 4, opts.upper));
            putchar(hex_digit(data[pos + i] & 0xf, opts.upper));
        }
        putchar('\n');
        pos += static_cast<size_t>(chunk);
        if (opts.length >= 0) { remaining -= chunk; }
    }
    return XXD_OK;
}

/* ── C include (-i) mode ───────────────────────────────────────────────── */

int run_c_include(const std::vector<uint8_t>& data, const XxdOptions& opts,
                  const char* infile) {
    int const cols = opts.cols > 0 ? opts.cols : CINCLUDE_COLS;
    std::string name = opts.var_name.empty() ? derive_var_name(infile) : opts.var_name;
    if (opts.capitalize) {
        for (char& c : name) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
    }

    long const start = opts.seek >= 0 ? opts.seek : 0;
    size_t const begin = static_cast<size_t>(start);
    size_t count = (begin < data.size()) ? (data.size() - begin) : 0;
    if (opts.length >= 0 && static_cast<size_t>(opts.length) < count) {
        count = static_cast<size_t>(opts.length);
    }

    /* With -t a terminating 0x00 byte is appended to the array; it counts as a
     * normal element for line breaking and comma placement, which is why the
     * comma can end up after the last data byte. */
    size_t const total = count + (opts.terminate ? 1U : 0U);

    printf("unsigned char %s[] = {\n", name.c_str());
    if (count == 0 && opts.terminate) {
        /* The reference folds the lone terminator onto the "};" line. */
        printf("  0x00};\n");
        printf("unsigned int %s_%s = %zu;\n", name.c_str(),
               opts.capitalize ? "LEN" : "len", count);
        return XXD_OK;
    }
    for (size_t i = 0; i < total; i++) {
        if (i % static_cast<size_t>(cols) == 0) { printf("  "); }
        if (i < count) {
            uint8_t const b = data[begin + i];
            printf("0x%c%c", hex_digit(b >> 4, opts.upper),
                   hex_digit(b & 0xf, opts.upper));
        } else {
            printf("0x00");
        }
        /* Every element except the last overall is followed by ", " — or by a
         * bare "," when the line breaks here. */
        bool const last = (i + 1 == total);
        if (!last) {
            if ((i + 1) % static_cast<size_t>(cols) == 0) { printf(","); }
            else { printf(", "); }
        }
        if ((i + 1) % static_cast<size_t>(cols) == 0) { putchar('\n'); }
    }
    if (total > 0 && total % static_cast<size_t>(cols) != 0) { putchar('\n'); }
    printf("};\n");
    printf("unsigned int %s_%s = %zu;\n", name.c_str(),
           opts.capitalize ? "LEN" : "len", count);
    return XXD_OK;
}

/* ── Reverse (-r) mode ─────────────────────────────────────────────────── */

int hexval(int c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

/* True when `f` is a regular file: the only kind of stream the reverse
 * parsers can reposition. Pipes and terminals are not seekable, so output
 * going there must be written strictly in order. */
bool is_seekable(FILE* f) {
    struct stat st;
    // NOLINTNEXTLINE(misc-include-cleaner)
    if (fstat(fileno(f), &st) != 0) { return false; }
    return S_ISREG(st.st_mode);
}

/* Parse default/`-i` dumps: "00000000: 4142 4344  ABCD". */
int reverse_default(std::FILE* in, std::FILE* out, const XxdOptions& opts) {
    bool const seekable = is_seekable(out);
    char line[4096];
    long sequential = 0;
    while (fgets(line, sizeof(line), in) != nullptr) {
        char* p = line;
        while (*p == ' ' || *p == '\t') { p++; }
        if (*p == '\0' || *p == '\n') { continue; }

        long offset = 0;
        bool has_offset = false;
        char* end = nullptr;
        /* Try to parse a leading hex offset followed by ':'. */
        errno = 0;
        offset = strtol(p, &end, 16);
        if (end != p && *end == ':') {
            has_offset = true;
            p = end + 1;
        } else {
            offset = 0;
        }

        /* Skip to the hex field, but stop before the ASCII gutter. The gutter
         * begins after at least two spaces following the hex bytes. We parse
         * hex pairs, tolerating the group spaces. */
        int hi = -1;
        long byte_index = 0;
        for (; *p != '\0' && *p != '\n'; p++) {
            int const v = hexval(static_cast<unsigned char>(*p));
            if (v < 0) {
                if (*p == ' ') {
                    /* Two consecutive spaces mark the start of the gutter. */
                    if (p[1] == ' ') { break; }
                    continue;
                }
                if (*p == '*' ) { break; } /* autoskip marker, nothing to emit */
                continue;
            }
            if (hi < 0) {
                hi = v;
            } else {
                int const byte = (hi << 4) | v;
                hi = -1;
                long const target = has_offset
                    ? (offset + opts.offset_bias + byte_index)
                    : (sequential + opts.offset_bias);
                if (out != nullptr) {
                    /* Position the restore at its dump offset, but only when
                     * the destination can be repositioned. A pipe or a terminal
                     * is not seekable, so restore into those sequentially. */
                    if (seekable && has_offset) {
                        if (fseek(out, target, SEEK_SET) != 0) {
                            (void)fprintf(stderr,
                                "xxd: Sorry, cannot seek backwards.\n");
                            return XXD_USAGE;
                        }
                    }
                    (void)putc(byte, out);
                }
                byte_index++;
            }
        }
        if (has_offset) { sequential = offset + byte_index; }
        else { sequential += byte_index; }
    }
    return XXD_OK;
}

/* Parse `-ps` plain hex: ignoring all non-hex characters. */
int reverse_plain(std::FILE* in, std::FILE* out, const XxdOptions& opts) {
    (void)opts;
    int c;
    int hi = -1;
    while ((c = getc(in)) != EOF) {
        int const v = hexval(c);
        if (v < 0) { continue; }
        if (hi < 0) {
            hi = v;
        } else {
            if (out != nullptr) { (void)putc((hi << 4) | v, out); }
            hi = -1;
        }
    }
    if (opts.terminate) { /* reserved */ }
    return XXD_OK;
}

/* Parse `-i` C include output: "  0x48, 0x65, ...". */
int reverse_c_include(std::FILE* in, std::FILE* out, const XxdOptions& opts) {
    (void)opts;
    char line[4096];
    while (fgets(line, sizeof(line), in) != nullptr) {
        char* p = line;
        while (*p != '\0') {
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                int const hi = hexval(static_cast<unsigned char>(p[2]));
                int const lo = hexval(static_cast<unsigned char>(p[3]));
                if (hi >= 0 && lo >= 0) {
                    if (out != nullptr) { (void)putc((hi << 4) | lo, out); }
                    p += 4;
                    continue;
                }
            }
            p++;
        }
    }
    return XXD_OK;
}

/* ── Usage / help ──────────────────────────────────────────────────────── */

void print_help(const char* prog) {
    printf("Usage:\n       %s [options] [infile [outfile]]\n", prog);
    printf("    or\n       %s -r [-s [-]offset] [-c cols] [-ps] [infile [outfile]]\n", prog);
    printf("Options:\n");
    printf("    -a          toggle autoskip: A single '*' replaces nul-lines. Default off.\n");
    printf("    -b          binary digit dump (incompatible with -ps). Default hex.\n");
    printf("    -C          capitalize variable names in C include file style (-i).\n");
    printf("    -c cols     format <cols> octets per line. Default 16 (-i: 12, -ps: 30).\n");
    printf("    -E          show characters in EBCDIC. Default ASCII.\n");
    printf("    -e          little-endian dump (incompatible with -ps,-i,-r).\n");
    printf("    -g bytes    number of octets per group in normal output. Default 2 (-e: 4).\n");
    printf("    -h          print this summary.\n");
    printf("    -i          output in C include file style.\n");
    printf("    -t          append terminating zero to C include output (-i).\n");
    printf("    -l len      stop after <len> octets.\n");
    printf("    -n name     set the variable name used in C include output (-i).\n");
    printf("    -o off      add <off> to the displayed file position.\n");
    printf("    -ps         output in postscript plain hexdump style.\n");
    printf("    -r          reverse operation: convert (or patch) hexdump into binary.\n");
    printf("    -r -s off   revert with <off> added to file positions found in hexdump.\n");
    printf("    -d          show offset in decimal instead of hex.\n");
    printf("    -s [+][-]seek  start at <seek> bytes abs. (or +: rel.) infile offset.\n");
    printf("    -u          use upper case hex letters.\n");
    printf("    -R when     colorize the output; <when> can be 'always', 'auto' or 'never'. Default: 'auto'.\n");
    printf("    -v          show version: \"%s (modbox) 1.0\".\n", prog);
}

/* Parse an offset/length argument the way the reference does: base 0, so
 * "0x10" is hex, "010" is octal and "10" is decimal. A leading '+' or '-' is
 * accepted (the sign is meaningful for -s, and handled by the caller). */
int parse_long(const char* s, long& out) {
    if (s == nullptr || *s == '\0') { return -1; }
    char* end = nullptr;
    errno = 0;
    long const v = strtol(s, &end, 0);
    if (errno != 0 || end == s) { return -1; }
    out = v;
    return 0;
}

int parse_int(const char* s, int& out) {
    long tmp = 0;
    if (parse_long(s, tmp) != 0) { return -1; }
    out = static_cast<int>(tmp);
    return 0;
}

/* Split `-ps` back out of argv. argtable3 only knows single-character short
 * options, so this two-letter spelling is recognised here and removed from the
 * list argtable gets to see. */
bool extract_plain_flag(int argc, char** argv, std::vector<char*>& args) {
    args.reserve(static_cast<size_t>(argc));
    bool plain_flag = false;
    for (int i = 0; i < argc; i++) {
        if (i > 0 && argv[i] != nullptr && strcmp(argv[i], "-ps") == 0) {
            plain_flag = true;
            continue;   /* drop it from the parsed argument list */
        }
        args.push_back(argv[i]);
    }
    return plain_flag;
}

/* Copy the numeric/string option values out of argtable into `opts`. Every
 * option is optional, so only the ones actually given are consulted. */
void parse_numeric_options(struct arg_str* opt_cols, struct arg_str* opt_group,
                           struct arg_str* opt_len, struct arg_str* opt_off,
                           struct arg_str* opt_seek, struct arg_str* opt_name,
                           struct arg_str* opt_color, XxdOptions& opts) {
    if (opt_cols->count > 0) { (void)parse_int(opt_cols->sval[0], opts.cols); }
    if (opt_group->count > 0) {
        (void)parse_int(opt_group->sval[0], opts.group);
        opts.group_set = true;
    }
    if (opt_len->count > 0) { (void)parse_long(opt_len->sval[0], opts.length); }
    if (opt_off->count > 0) { (void)parse_long(opt_off->sval[0], opts.offset_bias); }
    if (opt_name->count > 0) { opts.var_name = opt_name->sval[0]; }
    if (opt_seek->count > 0) {
        const char* s = opt_seek->sval[0];
        if (*s == '+') { opts.seek_sign = 1; s++; }
        else if (*s == '-') { opts.seek_sign = -1; s++; }
        (void)parse_long(s, opts.seek);
    }
    if (opt_color->count > 0) {
        const char* w = opt_color->sval[0];
        if (strcmp(w, "never") == 0) { opts.color = 0; }
        else if (strcmp(w, "always") == 0) { opts.color = 1; }
        else { opts.color = -1; }
    }
}

/* Reject option combinations the reference tool also rejects. Returns XXD_OK
 * when the combination is usable, XXD_USAGE otherwise (after printing help or
 * a specific diagnostic). */
int check_compatibility(const XxdOptions& opts, const char* prog) {
    if (opts.binary && opts.plain) { print_help(prog); return XXD_USAGE; }
    if (opts.little_endian && (opts.plain || opts.c_include || opts.reverse)) {
        print_help(prog);
        return XXD_USAGE;
    }
    /* -e reverses bytes inside each group; that is only well defined when the
     * group is a power of two (or 0, meaning "the whole line"). */
    if (opts.little_endian && opts.group_set && opts.group != 0 &&
        (opts.group & (opts.group - 1)) != 0) {
        (void)fprintf(stderr,
                      "xxd: number of octets per group must be a power of 2 with -e.\n");
        return XXD_USAGE;
    }
    return XXD_OK;
}

/* Run the requested reverse parser, writing to stdout or patching `outfile`. */
int run_reverse(FILE* in, const char* outfile, const XxdOptions& opts) {
    FILE* out = stdout;
    if (outfile != nullptr) {
        /* Patching: if the file exists open r+b, else create w+b. */
        out = fopen(outfile, "r+b");
        if (out == nullptr) { out = fopen(outfile, "w+b"); }
        if (out == nullptr) {
            (void)fprintf(stderr, "xxd: %s: %s\n", outfile, strerror(errno));
            if (in != stdin) { (void)fclose(in); }
            return XXD_IOERR;
        }
    }
    int rc;
    if (opts.plain) {
        rc = reverse_plain(in, out, opts);
    } else if (opts.c_include) {
        rc = reverse_c_include(in, out, opts);
    } else {
        rc = reverse_default(in, out, opts);
    }
    if (in != stdin) { (void)fclose(in); }
    if (out != stdout) { (void)fclose(out); }
    return rc;
}

}  // namespace

/* ── Main command ──────────────────────────────────────────────────────── */

int xxd_command(int argc, char** argv) {
    const char* prog = (argc > 0 && argv[0] != nullptr) ? argv[0] : "xxd";

    /* `-ps` is a two-letter option that argtable3 (single-char short options)
     * cannot express. Pull it out of argv before parsing and note it as a flag;
     * the remaining arguments are handed to argtable unchanged. */
    std::vector<char*> args;
    bool const plain_flag = extract_plain_flag(argc, argv, args);

    struct arg_lit* opt_autoskip = arg_lit0("a", nullptr, "toggle autoskip");
    struct arg_lit* opt_binary = arg_lit0("b", nullptr, "binary digit dump");
    struct arg_lit* opt_capitalize = arg_lit0("C", nullptr, "capitalize variable names");
    struct arg_str* opt_cols = arg_str0("c", nullptr, "cols", "octets per line");
    struct arg_lit* opt_decimal = arg_lit0("d", nullptr, "decimal offsets");
    struct arg_lit* opt_ebcdic = arg_lit0("E", nullptr, "EBCDIC gutter");
    struct arg_lit* opt_endian = arg_lit0("e", nullptr, "little-endian dump");
    struct arg_str* opt_group = arg_str0("g", nullptr, "bytes", "octets per group");
    struct arg_lit* opt_help = arg_lit0("h", "help", "print summary");
    struct arg_lit* opt_cinclude = arg_lit0("i", nullptr, "C include style");
    struct arg_str* opt_len = arg_str0("l", nullptr, "len", "stop after len octets");
    struct arg_str* opt_name = arg_str0("n", nullptr, "name", "C variable name");
    struct arg_str* opt_off = arg_str0("o", nullptr, "off", "add off to displayed offset");
    struct arg_lit* opt_reverse = arg_lit0("r", nullptr, "reverse operation");
    struct arg_str* opt_color = arg_str0("R", nullptr, "when", "colorize output");
    struct arg_str* opt_seek = arg_str0("s", nullptr, "seek", "start offset");
    struct arg_lit* opt_terminate = arg_lit0("t", nullptr, "append terminating zero");
    struct arg_lit* opt_upper = arg_lit0("u", nullptr, "upper case hex");
    struct arg_lit* opt_version = arg_lit0("v", "version", "show version");
    struct arg_file* operands = arg_filen(nullptr, nullptr, "infile outfile", 0, 2, "files");
    struct arg_end* end = arg_end(20);

    ArgTable table({
        opt_autoskip, opt_binary, opt_capitalize, opt_cols, opt_decimal,
        opt_ebcdic, opt_endian, opt_group, opt_help, opt_cinclude, opt_len,
        opt_name, opt_off, opt_reverse, opt_color, opt_seek,
        opt_terminate, opt_upper, opt_version, operands, end
    });

    int const nerrors = table.parse(static_cast<int>(args.size()), args.data());

    if (opt_help->count > 0) {
        print_help(prog);
        return XXD_OK;
    }
    if (opt_version->count > 0) {
        print_version("xxd");
        return XXD_OK;
    }
    if (nerrors > 0) {
        (void)fprintf(stderr, "xxd: invalid option or argument\n");
        print_help(prog);
        return XXD_USAGE;
    }

    XxdOptions opts;
    opts.autoskip = opt_autoskip->count > 0;
    opts.binary = opt_binary->count > 0;
    opts.capitalize = opt_capitalize->count > 0;
    opts.decimal_offsets = opt_decimal->count > 0;
    opts.ebcdic = opt_ebcdic->count > 0;
    opts.little_endian = opt_endian->count > 0;
    opts.c_include = opt_cinclude->count > 0;
    opts.plain = plain_flag;
    opts.reverse = opt_reverse->count > 0;
    opts.terminate = opt_terminate->count > 0;
    opts.upper = opt_upper->count > 0;

    parse_numeric_options(opt_cols, opt_group, opt_len, opt_off, opt_seek,
                          opt_name, opt_color, opts);

    /* Incompatible combinations, per the reference. */
    if (check_compatibility(opts, prog) != XXD_OK) { return XXD_USAGE; }

    const char* infile = (operands->count >= 1) ? operands->filename[0] : "-";
    const char* outfile = (operands->count >= 2) ? operands->filename[1] : nullptr;

    FILE* in = stdin;
    if (strcmp(infile, "-") != 0) {
        in = fopen(infile, "rb");
        if (in == nullptr) {
            (void)fprintf(stderr, "xxd: %s: %s\n", infile, strerror(errno));
            return XXD_IOERR;
        }
    }

    std::vector<uint8_t> data;

    if (opts.reverse) {
        return run_reverse(in, outfile, opts);
    }

    if (read_all(in, data) != 0) {
        (void)fprintf(stderr, "xxd: %s: %s\n", infile, strerror(errno));
        if (in != stdin) { (void)fclose(in); }
        return XXD_IOERR;
    }
    if (in != stdin) { (void)fclose(in); }

    /* -s with a backwards sign in dump mode is a reference error. A relative
     * (+) seek from the start of a freshly read file is the same as an
     * absolute one, so only the backwards case needs handling. */
    if (opts.seek_sign < 0) {
        (void)fprintf(stderr, "xxd: Sorry, cannot seek backwards.\n");
        return XXD_USAGE;
    }

    if (opts.plain) {
        return run_plain(data, opts);
    }
    if (opts.c_include) {
        return run_c_include(data, opts, infile);
    }
    return run_dump(data, opts);
}

REGISTER_COMMAND("xxd", xxd_command, "Make a hexdump or do the reverse")
