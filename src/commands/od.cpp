#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <ctime>
#include <argtable3.h>

#include "commands/od.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

namespace {

enum class AddressBase {
    octal,
    decimal,
    hex,
    none
};

enum class DumpFormat {
    octal_byte,
    octal_short,
    octal_long,
    unsigned_decimal,
    signed_decimal,
    hex_byte,
    hex_short,
    hex_long,
    char_display
};

struct OdOptions {
    AddressBase address_base = AddressBase::octal;
    std::vector<std::pair<DumpFormat, int>> formats;
    int skip_bytes = 0;
    int width_override = 0;
    int output_width = 16;
};

uint16_t od_checksum16(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return static_cast<uint16_t>(sum & 0xFFFF);
}

uint32_t od_checksum32(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// Print one formatted unit of `fmt` covering bytes [i, i+bytes_per_unit).
// Missing trailing bytes are rendered as blanks so columns stay aligned.
void print_unit(const uint8_t* data, size_t len, size_t i,
                DumpFormat fmt, int bytes_per_unit) {
    size_t chunk = len - i;
    if (bytes_per_unit > 0 && chunk > static_cast<size_t>(bytes_per_unit)) {
        chunk = static_cast<size_t>(bytes_per_unit);
    }
    if (bytes_per_unit <= 0) { bytes_per_unit = 1; }

    switch (fmt) {
        case DumpFormat::octal_byte: {
            for (size_t j = 0; j < chunk; j++) { printf(" %03o", data[i + j]); }
            for (size_t j = chunk; j < static_cast<size_t>(bytes_per_unit); j++) {
                printf("    ");
            }
            break;
        }
        case DumpFormat::octal_short: {
            uint16_t val = 0;
            size_t const n = chunk < 2 ? chunk : 2;
            for (size_t j = 0; j < n; j++) {
                val |= static_cast<uint16_t>(data[i + j]) << (8 * j);
            }
            printf(" %06o", val);
            break;
        }
        case DumpFormat::octal_long: {
            if (bytes_per_unit >= 8) {
                uint64_t val = 0;
                size_t const n = chunk < 8 ? chunk : 8;
                for (size_t j = 0; j < n; j++) {
                    val |= static_cast<uint64_t>(data[i + j]) << (8 * j);
                }
                printf(" %022llo", static_cast<unsigned long long>(val));
            } else {
                uint32_t val = 0;
                size_t const n = chunk < 4 ? chunk : 4;
                for (size_t j = 0; j < n; j++) {
                    val |= static_cast<uint32_t>(data[i + j]) << (8 * j);
                }
                printf(" %011o", val);
            }
            break;
        }
        case DumpFormat::unsigned_decimal:
        case DumpFormat::signed_decimal: {
            bool const is_signed = (fmt == DumpFormat::signed_decimal);
            if (bytes_per_unit == 1) {
                for (size_t j = 0; j < chunk; j++) {
                    if (is_signed) {
                        printf(" %4d", static_cast<int8_t>(data[i + j]));
                    } else {
                        printf(" %3u", data[i + j]);
                    }
                }
                for (size_t j = chunk; j < static_cast<size_t>(bytes_per_unit); j++) {
                    printf("    ");
                }
            } else if (bytes_per_unit == 2) {
                uint16_t uval = 0;
                size_t const n = chunk < 2 ? chunk : 2;
                for (size_t j = 0; j < n; j++) {
                    uval |= static_cast<uint16_t>(data[i + j]) << (8 * j);
                }
                if (is_signed) {
                    printf(" %6d", static_cast<int16_t>(uval));
                } else {
                    printf(" %5u", uval);
                }
            } else {
                uint32_t uval = 0;
                size_t const n = chunk < 4 ? chunk : 4;
                for (size_t j = 0; j < n; j++) {
                    uval |= static_cast<uint32_t>(data[i + j]) << (8 * j);
                }
                if (is_signed) {
                    printf(" %11d", static_cast<int32_t>(uval));
                } else {
                    printf(" %10u", uval);
                }
            }
            break;
        }
        case DumpFormat::hex_byte: {
            for (size_t j = 0; j < chunk; j++) { printf(" %02x", data[i + j]); }
            for (size_t j = chunk; j < static_cast<size_t>(bytes_per_unit); j++) {
                printf("   ");
            }
            break;
        }
        case DumpFormat::hex_short: {
            uint16_t val = 0;
            size_t const n = chunk < 2 ? chunk : 2;
            for (size_t j = 0; j < n; j++) {
                val |= static_cast<uint16_t>(data[i + j]) << (8 * j);
            }
            printf(" %04x", val);
            break;
        }
        case DumpFormat::hex_long: {
            if (bytes_per_unit >= 8) {
                uint64_t val = 0;
                size_t const n = chunk < 8 ? chunk : 8;
                for (size_t j = 0; j < n; j++) {
                    val |= static_cast<uint64_t>(data[i + j]) << (8 * j);
                }
                printf(" %016llx", static_cast<unsigned long long>(val));
            } else if (bytes_per_unit == 4) {
                uint32_t val = 0;
                size_t const n = chunk < 4 ? chunk : 4;
                for (size_t j = 0; j < n; j++) {
                    val |= static_cast<uint32_t>(data[i + j]) << (8 * j);
                }
                printf(" %08x", val);
            } else if (chunk >= 2) {
                uint16_t const val = static_cast<uint16_t>(data[i]) |
                                     (static_cast<uint16_t>(data[i + 1]) << 8);
                printf(" %04x", val);
            } else if (chunk == 1) {
                printf(" %02x", data[i]);
            } else {
                printf("         ");
            }
            break;
        }
        case DumpFormat::char_display: {
            for (size_t j = 0; j < chunk; j++) {
                uint8_t const c = data[i + j];
                if (c >= 32 && c <= 126) {
                    printf(" %3c", c);
                } else {
                    char esc[8];
                    switch (c) {
                        case '\0': snprintf(esc, sizeof(esc), "\\0"); break;
                        case '\a': snprintf(esc, sizeof(esc), "\\a"); break;
                        case '\b': snprintf(esc, sizeof(esc), "\\b"); break;
                        case '\f': snprintf(esc, sizeof(esc), "\\f"); break;
                        case '\n': snprintf(esc, sizeof(esc), "\\n"); break;
                        case '\r': snprintf(esc, sizeof(esc), "\\r"); break;
                        case '\t': snprintf(esc, sizeof(esc), "\\t"); break;
                        case '\v': snprintf(esc, sizeof(esc), "\\v"); break;
                        case 127:  snprintf(esc, sizeof(esc), "177"); break;
                        default:   snprintf(esc, sizeof(esc), "%03o", c); break;
                    }
                    printf(" %3s", esc);
                }
            }
            for (size_t j = chunk; j < static_cast<size_t>(bytes_per_unit); j++) {
                printf("    ");
            }
            break;
        }
    }
}

void print_offset(uint64_t off, AddressBase base) {
    if (base == AddressBase::none) { return; }
    switch (base) {
        case AddressBase::decimal: printf("%07llu", static_cast<unsigned long long>(off)); break;
        case AddressBase::hex:     printf("%06llx", static_cast<unsigned long long>(off)); break;
        default:                   printf("%07llo", static_cast<unsigned long long>(off)); break;
    }
}

void dump_buffer(const uint8_t* data, size_t len, uint64_t offset,
                        OdOptions& opts) {
    if (opts.formats.empty()) {
        opts.formats.push_back({DumpFormat::octal_short, 2});
    }

    // Print one output line per `output_width` bytes, one line per format.
    for (size_t line_start = 0; line_start < len; ) {
        size_t const line_len = std::min(static_cast<size_t>(opts.output_width),
                                         len - line_start);
        for (const auto& fmt : opts.formats) {
            print_offset(offset + line_start, opts.address_base);
            for (size_t j = 0; j < line_len; j += static_cast<size_t>(fmt.second > 0 ? fmt.second : 1)) {
                print_unit(data + line_start, line_len, j, fmt.first, fmt.second);
            }
            printf("\n");
        }
        line_start += line_len;
    }
}

void print_last_offset(uint64_t offset, AddressBase base) {
    if (base == AddressBase::none) { return; }
    print_offset(offset, base);
    printf("\n");
}

void dump_file(FILE* fp, const char* filename, OdOptions& opts) {
    const size_t buf_size = 8192;
    std::vector<uint8_t> buf(buf_size);
    uint64_t offset = 0;

    while (true) {
        size_t const n = fread(buf.data(), 1, buf_size, fp);
        if (n == 0) { break;
}
        dump_buffer(buf.data(), n, offset, opts);
        offset += n;
    }

    if (ferror(fp) != 0) {
        (void)fprintf(stderr, "od: %s: read error: %s\n", (filename != nullptr) ? filename : "-", strerror(errno));
    }

    print_last_offset(offset, opts.address_base);
}

// Parse a single -t TYPE string, e.g. "x1", "o2", "c", "d4", "x1z".
// GNU od accepts one type per -t option: a type char followed by an optional
// size (C=1, S=2, I=4, L=8, or a decimal byte count).
bool parse_format_string(const char* str, OdOptions& opts) {
    if ((str == nullptr) || ((*str) == 0)) { return false;
}

    size_t i = 0;
    char const type = str[i++];

    // Determine element size from the optional suffix.
    int size = 0;
    if (str[i] != '\0') {
        switch (str[i]) {
            case 'C': size = 1; i++; break;
            case 'S': size = 2; i++; break;
            case 'I': size = 4; i++; break;
            case 'L': size = 8; i++; break;
            default:
                if (str[i] >= '0' && str[i] <= '9') {
                    size = std::atoi(str + i);
                    while (str[i] >= '0' && str[i] <= '9') { i++; }
                }
                break;
        }
    }

    // 'z' suffix: append a printable-character column. Recognized but the
    // extra column is intentionally not emitted (kept simple).
    while (str[i] != '\0') { i++; }

    switch (type) {
        case 'a':
        case 'c':
            opts.formats.push_back({DumpFormat::char_display, 1});
            break;
        case 'd':
            opts.formats.push_back({DumpFormat::signed_decimal, size > 0 ? size : 2});
            break;
        case 'u':
            opts.formats.push_back({DumpFormat::unsigned_decimal, size > 0 ? size : 2});
            break;
        case 'o':
            if (size >= 8) {
                opts.formats.push_back({DumpFormat::octal_long, 8});
            } else if (size == 4) {
                opts.formats.push_back({DumpFormat::octal_long, 4});
            } else if (size == 2) {
                opts.formats.push_back({DumpFormat::octal_short, 2});
            } else {
                opts.formats.push_back({DumpFormat::octal_byte, 1});
            }
            break;
        case 'x':
            if (size >= 8) {
                opts.formats.push_back({DumpFormat::hex_long, 8});
            } else if (size == 4) {
                opts.formats.push_back({DumpFormat::hex_long, 4});
            } else if (size == 2) {
                opts.formats.push_back({DumpFormat::hex_short, 2});
            } else {
                opts.formats.push_back({DumpFormat::hex_byte, 1});
            }
            break;
        default:
            return false;
    }

    return true;
}

}

int od_command(int argc, char** argv) {
    // Normalize bundled option arguments that GNU od accepts but argtable3
    // cannot express directly: "-A<x>" -> "-A" "<x>" and "-t<fmt>" -> "-t" "<fmt>".
    std::vector<std::string> owned;
    std::vector<char*> norm;
    owned.reserve(static_cast<size_t>(argc) * 2);
    norm.reserve(static_cast<size_t>(argc) * 2);
    norm.push_back(argv[0]);
    for (int a = 1; a < argc; a++) {
        const char* arg = argv[a];
        if ((arg[0] == '-' && arg[1] == 'A' && arg[2] != '\0') ||
            (arg[0] == '-' && arg[1] == 't' && arg[2] != '\0')) {
            owned.emplace_back(arg, 2);      // "-A" or "-t"
            owned.emplace_back(arg + 2);     // the value
        } else {
            owned.emplace_back(arg);
        }
    }
    for (std::string& s : owned) { norm.push_back(s.data()); }
    int const nargc = static_cast<int>(norm.size());
    char** const nargv = norm.data();

    struct arg_str* address_opt = arg_str0("A", "address-radix", "<base>", "output address in octal");
    struct arg_str* format_opt = arg_strn("t", "format", "<format>", 0, 10, "output format");
    struct arg_lit* chartype_opt = arg_lit0("c", NULL, "print printable characters");
    struct arg_lit* octal_byte_opt = arg_lit0("b", NULL, "octal bytes");
    struct arg_lit* octal_short_opt = arg_lit0(NULL, "o", "octal 2-byte units");
    struct arg_lit* octal_long_opt = arg_lit0(NULL, "l", "octal 4-byte units");
    struct arg_lit* hex_byte_opt = arg_lit0("x", NULL, "hex bytes");
    struct arg_lit* unsigned_byte_opt = arg_lit0(NULL, "d", "unsigned decimal 2-byte units");
    struct arg_lit* skip_bytes_opt = arg_lit0(NULL, "skip-bytes", "skip bytes (not fully implemented)");
    struct arg_lit* width_opt = arg_lit0(NULL, "width", "output width (not fully implemented)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input files");
    struct arg_end* end = arg_end(20);

    ArgTable at({address_opt, format_opt, chartype_opt, octal_byte_opt,
                 octal_short_opt, octal_long_opt, hex_byte_opt,
                 unsigned_byte_opt, skip_bytes_opt, width_opt, help_opt,
                 files_arg, end});

    int const nerrors = at.parse(nargc, nargv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Dump files in octal, hex, or other formats.\n");
        printf("\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        printf("\n");
        printf("Address base:\n");
        printf("  -A o         output address in octal (default)\n");
        printf("  -A d         output address in decimal\n");
        printf("  -A x         output address in hex\n");
        printf("  -A n         no address output\n");
        printf("\n");
        printf("Output format (-t format, and shortcuts):\n");
        printf("  -b           octal bytes\n");
        printf("  -c           printable characters\n");
        printf("  -d           unsigned decimal 2-byte units\n");
        printf("  -o           octal 2-byte units\n");
        printf("  -x           hex bytes\n");
        printf("  o[1][b|l]    octal bytes/shorts/longs\n");
        printf("  d[1][b|l]    unsigned decimal\n");
        printf("  x[1][b|l]    hex bytes/shorts/longs\n");
        printf("  c           printable characters\n");
        printf("\n");
        printf("  -h, --help   display this help and exit\n");
        printf("\n");
        printf("Default is equivalent to: -A o -t o2\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    OdOptions opts;

    if (address_opt->count > 0) {
        const char* val = address_opt->sval[0];
        if (strcmp(val, "d") == 0) { opts.address_base = AddressBase::decimal;
        } else if (strcmp(val, "x") == 0) { opts.address_base = AddressBase::hex;
        } else if (strcmp(val, "n") == 0) { opts.address_base = AddressBase::none;
        } else if (strcmp(val, "o") == 0) { opts.address_base = AddressBase::octal;
}
    }

    // GNU-style format shortcuts. These are appended in command-line order,
    // matching od's behavior of accepting multiple / interleaved formats.
    if (chartype_opt->count > 0) { opts.formats.push_back({DumpFormat::char_display, 1}); }
    if (octal_byte_opt->count > 0) { opts.formats.push_back({DumpFormat::octal_byte, 1}); }
    if (octal_short_opt->count > 0) { opts.formats.push_back({DumpFormat::octal_short, 2}); }
    if (octal_long_opt->count > 0) { opts.formats.push_back({DumpFormat::octal_long, 4}); }
    if (hex_byte_opt->count > 0) { opts.formats.push_back({DumpFormat::hex_byte, 1}); }
    if (unsigned_byte_opt->count > 0) { opts.formats.push_back({DumpFormat::signed_decimal, 2}); }

    if (format_opt->count > 0) {
        for (int i = 0; i < format_opt->count; i++) {
            parse_format_string(format_opt->sval[i], opts);
        }
    }

    if (files_arg->count == 0) {
        dump_file(stdin, nullptr, opts);
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            const char* filename = files_arg->filename[i];
            if (strcmp(filename, "-") == 0) {
                dump_file(stdin, nullptr, opts);
            } else {
                FILE* fp = fopen(filename, "rb");
                if (fp == nullptr) {
                    (void)fprintf(stderr, "od: %s: %s\n", filename, strerror(errno));
                    continue;
                }
                dump_file(fp, filename, opts);
                (void)fclose(fp);
            }
        }
    }

    return 0;
}

REGISTER_COMMAND("od", od_command, "Dump files in octal, hex, or other formats");
