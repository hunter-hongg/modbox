#include <argtable3.h>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "commands/strings.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// GNU strings prints the offset right-aligned in a field wide enough for the
// largest octal offset a 32-bit file can hold (37777777 = 8 digits). Measured
// against binutils 2.46 the field is exactly 7 columns, left-padded with
// spaces, for every radix.
constexpr int kOffsetFieldWidth = 7;

enum class Radix { None, Octal, Decimal, Hex };

// The set of bytes GNU strings accepts inside a run, verified byte by byte
// against binutils 2.46: 0x20..0x7E plus TAB. Newline, carriage return, the
// remaining C0 controls, DEL and every byte with the high bit set all
// terminate a run.
bool is_printable(unsigned char c) { return (c >= 0x20 && c <= 0x7E) || c == '\t'; }

// -w additionally accepts the C0 whitespace controls TAB, LF, VT, FF, CR and
// space. Every other C0 control (including NUL, the remaining 0x01..0x08 and
// 0x0E..0x1F codes), DEL and every byte with the high bit set still break a
// run, so a run printed with -w contains no NUL and is always printable ASCII
// apart from its whitespace.
bool is_printable_with_whitespace(unsigned char c) {
    return is_printable(c) || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

void print_usage(const char* prog) {
    printf("Usage: %s [OPTION(S)] [FILE(S)]\n", prog);
    printf("Print printable strings in FILE(S) (standard input by default).\n");
    printf("\n");
    printf("  -a, --all              scan the entire file, not just the data section\n");
    printf("  -d, --data             only scan the data sections in the file\n");
    printf("  -f, --print-file-name  print the name of the file before each string\n");
    printf("  -n, --bytes=NUM        print sequences of at least NUM displayable\n");
    printf("                         characters (default 4)\n");
    printf("  -t, --radix=RADIX      print the location of each string in base\n");
    printf("                         8 (o), 10 (d) or 16 (x)\n");
    printf("  -w, --include-all-whitespace\n");
    printf("                         include all whitespace as valid string\n");
    printf("                         characters\n");
    printf("  -o                     an alias for --radix=o\n");
    printf("  -s, --output-separator=STRING\n");
    printf("                         separate strings in the output with STRING\n");
    printf("  -h, --help             display this help and exit\n");
    printf("  -V, --version          output version information and exit\n");
}

// Print offset in the requested radix, right-aligned in the fixed field.
void print_offset(uint64_t offset, Radix radix) {
    char buf[32];
    switch (radix) {
        case Radix::Octal:
            (void)snprintf(buf, sizeof(buf), "%llo", static_cast<unsigned long long>(offset));
            break;
        case Radix::Decimal:
            (void)snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(offset));
            break;
        case Radix::Hex:
            (void)snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(offset));
            break;
        case Radix::None:
            return;
    }
    const int len = static_cast<int>(strlen(buf));
    for (int i = len; i < kOffsetFieldWidth; i++) {
        (void)putchar(' ');
    }
    (void)fputs(buf, stdout);
}

struct StringsOptions {
    size_t min_length = 4;
    bool print_file_name = false;
    bool all_whitespace = false;
    Radix radix = Radix::None;
    std::string separator;
    bool has_separator = false;
};

// Scan one buffer and print every run of at least min_length printable bytes.
// The offset printed is the file-relative position of the run's first byte.
void scan_buffer(const unsigned char* data, size_t size, const std::string& name,
                 const StringsOptions* opts) {
    bool (*is_member)(unsigned char) =
        opts->all_whitespace ? &is_printable_with_whitespace : &is_printable;

    size_t i = 0;
    while (i < size) {
        if (!is_member(data[i])) {
            i++;
            continue;
        }
        const size_t start = i;
        while (i < size && is_member(data[i])) {
            i++;
        }
        if (i - start < opts->min_length) {
            continue;
        }
        if (opts->print_file_name) {
            printf("%s: ", name.c_str());
        }
        if (opts->radix != Radix::None) {
            print_offset(start, opts->radix);
            (void)putchar(' ');
        }
        (void)fwrite(data + start, 1, i - start, stdout);
        if (opts->has_separator) {
            (void)fputs(opts->separator.c_str(), stdout);
        } else {
            (void)putchar('\n');
        }
    }
}

bool read_stream(FILE* fp, std::vector<unsigned char>& out) {
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return ferror(fp) == 0;
}

bool read_path(const char* path, std::vector<unsigned char>& out) {
    struct stat st;
    if (stat(path, &st) != 0) {
        (void)fprintf(stderr, "strings: '%s': %s\n", path, strerror(errno));
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        (void)fprintf(stderr, "strings: Warning: '%s' is a directory\n", path);
        return false;
    }
    FILE* fp = fopen(path, "rb");
    if (fp == nullptr) {
        (void)fprintf(stderr, "strings: '%s': %s\n", path, strerror(errno));
        return false;
    }
    const bool ok = read_stream(fp, out);
    (void)fclose(fp);
    return ok;
}

int run_strings(int argc, char** argv) {
    struct arg_lit* all_opt = arg_lit0("a", "all", "scan the entire file, not just the data section");
    struct arg_lit* data_opt = arg_lit0("d", "data", "only scan the data sections in the file");
    struct arg_lit* fname_opt = arg_lit0("f", "print-file-name", "print the name of the file before each string");
    struct arg_str* bytes_opt = arg_str0("n", "bytes", "NUM", "print sequences of at least NUM displayable characters");
    struct arg_str* radix_opt = arg_str0("t", "radix", "RADIX", "print the location of each string in base 8, 10 or 16");
    struct arg_lit* ws_opt = arg_lit0("w", "include-all-whitespace", "include all whitespace as valid string characters");
    struct arg_lit* octal_opt = arg_lit0("o", nullptr, "an alias for --radix=o");
    struct arg_str* sep_opt = arg_str0("s", "output-separator", "STRING", "separate strings in the output with STRING");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_file* file_arg = arg_filen(nullptr, nullptr, "FILE", 0, 1000, "files to scan");
    struct arg_end* end = arg_end(20);

    ArgTable at({all_opt, data_opt, fname_opt, bytes_opt, radix_opt, ws_opt,
                 octal_opt, sep_opt, help_opt, version_opt, file_arg, end});
    const int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("strings");
        return 0;
    }

    if (nerrors > 0) {
        (void)print_arg_errors(end, argv[0]);
        print_usage(argv[0]);
        return 1;
    }

    StringsOptions opts;
    if (bytes_opt->count > 0) {
        // Take NUM as a string and parse it here rather than with arg_int0:
        // arg_int0 gates the value through strtol, which truncates anything
        // wider than long to int before this code ever sees it, and its error
        // wording does not name the option. Base 0 matches the reference's
        // acceptance of 0x / 0X hex, 0b binary and 0 octal prefixes.
        const char* text = bytes_opt->sval[0] != nullptr ? bytes_opt->sval[0] : "";
        // The reference reads NUM with bfd_scan_vma, which accepts an empty
        // argument as zero, so report it as too small rather than as malformed.
        if (text[0] == '\0') {
            (void)fprintf(stderr, "strings: minimum string length is too small: %s\n", text);
            return 1;
        }
        char* parse_end = nullptr;
        errno = 0;
        const long long value = strtoll(text, &parse_end, 0);
        if (parse_end == text || *parse_end != '\0') {
            (void)fprintf(stderr, "strings: invalid integer argument %s\n", text);
            print_usage(argv[0]);
            return 1;
        }
        // The reference keeps NUM in an unsigned 32-bit field, so it rejects
        // anything that does not fit and reserves a separate message for the
        // single largest value, which is the one that legitimately fits but
        // cannot describe a real run. A value too large for long long has
        // already saturated, so it fails the same range test.
        constexpr long long kMaxNum = 4294967294LL;  // 2^32 - 2
        if (errno == ERANGE || value > kMaxNum) {
            if (errno == 0 && value == kMaxNum + 1) {
                (void)fprintf(stderr, "strings: minimum string length %s is too big\n", text);
            } else {
                (void)fprintf(stderr, "strings: minimum string length is too big: %s\n", text);
            }
            return 1;
        }
        // Echo the argument as written, so a negative zero reports "-0".
        if (value < 1) {
            (void)fprintf(stderr, "strings: minimum string length is too %s: %s\n",
                          value == 0 ? "small" : "big", text);
            return 1;
        }
        opts.min_length = static_cast<size_t>(value);
    }

    // -t wins when both -t and -o are given, matching GNU.
    if (radix_opt->count > 0) {
        const char* value = radix_opt->sval[0] != nullptr ? radix_opt->sval[0] : "";
        if (value[0] == 'o' || value[0] == 'O') {
            opts.radix = Radix::Octal;
        } else if (value[0] == 'd' || value[0] == 'D') {
            opts.radix = Radix::Decimal;
        } else if (value[0] == 'x' || value[0] == 'X') {
            opts.radix = Radix::Hex;
        } else {
            (void)fprintf(stderr, "strings: invalid radix '%s', expected o, d or x\n", value);
            print_usage("strings");
            return 1;
        }
    } else if (octal_opt->count > 0) {
        opts.radix = Radix::Octal;
    }

    opts.print_file_name = (fname_opt->count > 0);
    opts.all_whitespace = (ws_opt->count > 0);
    if (sep_opt->count > 0) {
        opts.has_separator = true;
        opts.separator = sep_opt->sval[0] != nullptr ? sep_opt->sval[0] : "";
    }
    // -a and -d are accepted for compatibility; modbox always scans the whole
    // file, so both behave like -a. See the man page NOTES.

    std::vector<unsigned char> data;
    int status = 0;

    if (file_arg->count == 0) {
        if (!read_stream(stdin, data)) {
            (void)fprintf(stderr, "strings: standard input: %s\n", strerror(errno));
            return 1;
        }
        scan_buffer(data.data(), data.size(), "{standard input}", &opts);
        return 0;
    }

    for (int i = 0; i < file_arg->count; i++) {
        const char* name = file_arg->filename[i] != nullptr ? file_arg->filename[i] : "";
        data.clear();
        if (!read_path(name, data)) {
            status = 1;
            continue;
        }
        scan_buffer(data.data(), data.size(), name, &opts);
    }

    return status;
}

}  // namespace

int strings_command(int argc, char** argv) { return run_strings(argc, argv); }

REGISTER_COMMAND("strings", strings_command, "Print printable strings");
