#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <vector>
#include <ctime>
#include <argtable3.h>

#include "commands/od.hpp"
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

static uint16_t od_checksum16(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

static uint32_t od_checksum32(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static void dump_buffer(const uint8_t* data, size_t len, uint64_t offset,
                        OdOptions& opts) {
    if (opts.formats.empty()) {
        opts.formats.push_back({DumpFormat::octal_short, 2});
    }

    for (const auto& fmt : opts.formats) {
        const int bytes_per_unit = fmt.second;

        for (size_t i = 0; i < len; i += bytes_per_unit) {
            if (opts.address_base != AddressBase::none) {
                switch (opts.address_base) {
                    case AddressBase::octal:
                        printf("%07llu ", (unsigned long long)offset + i);
                        break;
                    case AddressBase::decimal:
                        printf("%07llu ", (unsigned long long)offset + i);
                        break;
                    case AddressBase::hex:
                        printf("%07x ", (unsigned long long)(offset + i));
                        break;
                    case AddressBase::none:
                        break;
                }
            }

            size_t chunk = len - i;
            if (bytes_per_unit > 0 && (int)chunk > bytes_per_unit) {
                chunk = bytes_per_unit;
            }

            switch (fmt.first) {
                case DumpFormat::octal_byte: {
                    for (size_t j = 0; j < chunk; j++) {
                        printf("%03o ", data[i + j]);
                    }
                    for (size_t j = chunk; j < (bytes_per_unit > 0 ? bytes_per_unit : 1); j++) {
                        printf("   ");
                    }
                    break;
                }
                case DumpFormat::octal_short: {
                    if (chunk >= 2) {
                        uint16_t val = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
                        printf("%06o ", val);
                    } else if (chunk == 1) {
                        printf("%06o ", data[i]);
                    }
                    break;
                }
                case DumpFormat::octal_long: {
                    if (chunk >= 4) {
                        uint32_t val = (uint32_t)data[i] | ((uint32_t)data[i + 1] << 8) |
                                       ((uint32_t)data[i + 2] << 16) | ((uint32_t)data[i + 3] << 24);
                        printf("%011o ", val);
                    } else if (chunk >= 2) {
                        uint16_t val = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
                        printf("%06o ", val);
                    } else if (chunk == 1) {
                        printf("%03o ", data[i]);
                    }
                    break;
                }
                case DumpFormat::unsigned_decimal: {
                    for (size_t j = 0; j < chunk; j++) {
                        printf("%03u ", data[i + j]);
                    }
                    break;
                }
                case DumpFormat::hex_byte: {
                    for (size_t j = 0; j < chunk; j++) {
                        printf("%02x ", data[i + j]);
                    }
                    for (size_t j = chunk; j < (bytes_per_unit > 0 ? bytes_per_unit : 1); j++) {
                        printf("   ");
                    }
                    break;
                }
                case DumpFormat::hex_short: {
                    if (chunk >= 2) {
                        uint16_t val = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
                        printf("%04x ", val);
                    } else if (chunk == 1) {
                        printf("%04x ", data[i]);
                    }
                    break;
                }
                case DumpFormat::hex_long: {
                    if (chunk >= 4) {
                        uint32_t val = (uint32_t)data[i] | ((uint32_t)data[i + 1] << 8) |
                                       ((uint32_t)data[i + 2] << 16) | ((uint32_t)data[i + 3] << 24);
                        printf("%08x ", val);
                    } else if (chunk >= 2) {
                        uint16_t val = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
                        printf("%04x ", val);
                    } else if (chunk == 1) {
                        printf("%02x ", data[i]);
                    }
                    break;
                }
                case DumpFormat::char_display: {
                    for (size_t j = 0; j < chunk; j++) {
                        uint8_t c = data[i + j];
                        if (c >= 32 && c <= 126) {
                            printf(" %c ", c);
                        } else {
                            switch (c) {
                                case '\a': printf("\\a "); break;
                                case '\b': printf("\\b "); break;
                                case '\f': printf("\\f "); break;
                                case '\n': printf("\\n "); break;
                                case '\r': printf("\\r "); break;
                                case '\t': printf("\\t "); break;
                                case '\v': printf("\\v "); break;
                                case 127:  printf("del "); break;
                                default:    printf("%03o ", c); break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    printf("\n");
}

static void dump_file(FILE* fp, const char* filename, OdOptions& opts) {
    const size_t buf_size = 8192;
    std::vector<uint8_t> buf(buf_size);
    uint64_t offset = 0;

    while (true) {
        size_t n = fread(buf.data(), 1, buf_size, fp);
        if (n == 0) break;
        dump_buffer(buf.data(), n, offset, opts);
        offset += n;
    }

    if (ferror(fp)) {
        fprintf(stderr, "od: %s: read error: %s\n", filename ? filename : "-", strerror(errno));
    }
}

static DumpFormat parse_format_char(char c) {
    switch (c) {
        case 'a': return DumpFormat::char_display;
        case 'c': return DumpFormat::char_display;
        case 'd': return DumpFormat::unsigned_decimal;
        case 'o': return DumpFormat::octal_byte;
        case 'u': return DumpFormat::unsigned_decimal;
        case 'x': return DumpFormat::hex_byte;
        default: return DumpFormat::octal_byte;
    }
}

static int parse_format_size(char c) {
    switch (c) {
        case 'C': case 'B': return 1;
        case 'S': case 'H': return 2;
        case 'L': case 'I': return 4;
        default: return 0;
    }
}

static bool parse_format_string(const char* str, OdOptions& opts) {
    if (!str || !*str) return false;

    size_t len = strlen(str);
    for (size_t i = 0; i < len; ) {
        if (str[i] == '_') {
            i++;
            continue;
        }

        DumpFormat fmt = parse_format_char(str[i]);
        int size = 0;

        if (i + 1 < len) {
            int next_size = parse_format_size(str[i + 1]);
            if (next_size > 0) {
                size = next_size;
                i++;
            }
        }

        switch (fmt) {
            case DumpFormat::octal_byte:
                if (size == 0) size = 1;
                opts.formats.push_back({fmt, size});
                break;
            case DumpFormat::hex_byte:
                if (size == 0) size = 1;
                opts.formats.push_back({fmt, size});
                break;
            case DumpFormat::char_display:
                opts.formats.push_back({fmt, 1});
                break;
            case DumpFormat::unsigned_decimal:
                opts.formats.push_back({fmt, size > 0 ? size : 1});
                break;
            default:
                opts.formats.push_back({fmt, size});
                break;
        }
        i++;
    }

    return true;
}

}

void od_command(int argc, char** argv) {
    struct arg_str* address_opt = arg_str0("A", "address-radix", "<base>", "output address in octal");
    struct arg_str* format_opt = arg_strn("t", "format", "<format>", 0, 10, "output format");
    struct arg_lit* skip_bytes_opt = arg_lit0(NULL, "skip-bytes", "skip bytes (not fully implemented)");
    struct arg_lit* width_opt = arg_lit0(NULL, "width", "output width (not fully implemented)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input files");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {address_opt, format_opt, skip_bytes_opt, width_opt, help_opt, files_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

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
        printf("Output format (-t format):\n");
        printf("  o[1][b|l]    octal bytes/shorts/longs\n");
        printf("  d[1][b|l]    unsigned decimal\n");
        printf("  x[1][b|l]    hex bytes/shorts/longs\n");
        printf("  c           printable characters\n");
        printf("\n");
        printf("  -h, --help   display this help and exit\n");
        printf("\n");
        printf("Default is equivalent to: -A o -t o2\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    OdOptions opts;

    if (address_opt->count > 0) {
        const char* val = address_opt->sval[0];
        if (strcmp(val, "d") == 0) opts.address_base = AddressBase::decimal;
        else if (strcmp(val, "x") == 0) opts.address_base = AddressBase::hex;
        else if (strcmp(val, "n") == 0) opts.address_base = AddressBase::none;
        else if (strcmp(val, "o") == 0) opts.address_base = AddressBase::octal;
    }

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
                if (!fp) {
                    fprintf(stderr, "od: %s: %s\n", filename, strerror(errno));
                    continue;
                }
                dump_file(fp, filename, opts);
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("od", od_command, "Dump files in octal, hex, or other formats");
