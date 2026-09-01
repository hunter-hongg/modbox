#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "commands/file.hpp"
#include "commands/command_macros.hpp"
#include "commands/arg_util.hpp"

namespace {

// ---------------------------------------------------------------------------
// Magic signatures
// ---------------------------------------------------------------------------

enum class FileType {
    EMPTY,
    ASCII_TEXT,
    UTF8_TEXT,
    SHELL_SCRIPT,
    ELF,
    GZIP,
    ZIP,
    UNKNOWN_DATA,
};

struct MagicEntry {
    const uint8_t* pattern;
    size_t len;
    size_t offset;
    FileType type;
    const char* description;
};

static const uint8_t kElfMagic[] = {0x7f, 'E', 'L', 'F'};
static const uint8_t kGzipMagic[] = {0x1f, 0x8b};
static const uint8_t kZipMagic[] = {0x50, 0x4b, 0x03, 0x04};
static const uint8_t kUtf8Bom[] = {0xef, 0xbb, 0xbf};
static const uint8_t kShebang[] = {'#', '!'};

static const std::vector<MagicEntry>& magic_table() {
    static const std::vector<MagicEntry> table = {
        { kGzipMagic,  2, 0, FileType::GZIP,      "gzip compressed data" },
        { kZipMagic,   4, 0, FileType::ZIP,       "ZIP archive data" },
    };
    return table;
}

static bool matches_pattern(const uint8_t* buf, size_t buf_len,
                            const MagicEntry& entry) {
    if (entry.offset + entry.len > buf_len) {
        return false;
    }
    return std::memcmp(buf + entry.offset, entry.pattern, entry.len) == 0;
}

static bool has_shebang(const uint8_t* data, size_t len, size_t offset) {
    if (offset + 2 > len) return false;
    return data[offset] == '#' && data[offset + 1] == '!';
}

// ---------------------------------------------------------------------------
// Text classification helpers
// ---------------------------------------------------------------------------

static std::string detect_line_endings(const uint8_t* data, size_t len) {
    if (len == 0) return "";
    bool has_crlf = false;
    bool has_lf = false;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\r' && i + 1 < len && data[i + 1] == '\n') {
            has_crlf = true;
            i++;
        } else if (data[i] == '\n') {
            has_lf = true;
        }
    }
    if (has_crlf && !has_lf) {
        return ", with CRLF line terminators";
    }
    return "";
}

static bool is_ascii_text(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = data[i];
        if (c == 0x00) return false;
        if (c == 0x09 || c == 0x0a || c == 0x0d || (c >= 0x20 && c <= 0x7e)) {
            continue;
        }
        if (c < 0x80) return false;
    }
    return true;
}

static std::string text_base_description(const uint8_t* data, size_t len) {
    if (is_ascii_text(data, len)) {
        return "ASCII text";
    }
    return "UTF-8 Unicode text";
}

static std::string shebang_description(const uint8_t* data, size_t len) {
    if (len <= 2) return "";
    size_t end = len;
    for (size_t j = 2; j < len; j++) {
        if (data[j] == '\n' || data[j] == '\r') {
            end = j;
            break;
        }
    }
    std::string line(reinterpret_cast<const char*>(data + 2), end - 2);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }
    if (line.empty()) {
        return ", script";
    }
    if (line.rfind("env", 0) == 0) {
        size_t start = line.find_first_not_of(' ');
        if (start != std::string::npos) {
            size_t sp = line.find_first_of(' ', start);
            std::string prog = (sp == std::string::npos)
                                  ? line.substr(start)
                                  : line.substr(start, sp - start);
            return ", script executable " + prog;
        }
        return ", script executable env";
    }
    return ", script executable " + line;
}

static std::string classify_text(const uint8_t* data, size_t len) {
    std::string desc = text_base_description(data, len);
    std::string le = detect_line_endings(data, len);
    if (!le.empty()) {
        desc += le;
    }
    if (has_shebang(data, len, 0)) {
        desc += shebang_description(data, len);
    }
    return desc;
}

// ---------------------------------------------------------------------------
// ELF classification
// ---------------------------------------------------------------------------

static std::string classify_elf(const uint8_t* data, size_t len, int class_byte) {
    std::string desc = (class_byte == 1) ? "ELF 32-bit" : "ELF 64-bit";

    if (len >= 20) {
        uint16_t machine = static_cast<uint16_t>(data[18]) |
                           (static_cast<uint16_t>(data[19]) << 8);
        uint16_t type = static_cast<uint16_t>(data[16]) |
                        (static_cast<uint16_t>(data[17]) << 8);
        const char* endianness =
            (data[5] == 1) ? "little-endian" : "big-endian";
        desc += " LSB " + std::string(endianness) + " executable";
        if (type == 3) {
            desc += " (dynamically linked)";
        } else if (type == 2) {
            desc += " (shared object)";
        }

        const char* arch = "unknown";
        switch (machine) {
        case 0x03: arch = "Intel 80386"; break;
        case 0x3E: arch = "x86-64"; break;
        case 0xB7: arch = "AArch64"; break;
        case 0x00: arch = "None"; break;
        default: break;
        }
        desc += ", " + std::string(arch);
    }
    return desc;
}

// ---------------------------------------------------------------------------
// Core classifier
// ---------------------------------------------------------------------------

static const size_t FILE_READ_BUF = 4096;

struct ClassifyResult {
    FileType type;
    std::string description;
};

ClassifyResult classify_bytes(const uint8_t* data, size_t len) {
    if (len == 0) {
        return { FileType::EMPTY, "empty" };
    }

    // ELF
    if (len >= 5 && data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' &&
        data[3] == 'F') {
        return { FileType::ELF, classify_elf(data, len, data[4]) };
    }

    // Binary magic entries
    for (const auto& entry : magic_table()) {
        if (matches_pattern(data, len, entry)) {
            return { entry.type, entry.description };
        }
    }

    // UTF-8 BOM
    if (matches_pattern(data, len, { kUtf8Bom, 3, 0, FileType::UTF8_TEXT, nullptr })) {
        return { FileType::UTF8_TEXT, classify_text(data, len) };
    }

    // Shebang or generic text
    return { FileType::ASCII_TEXT, classify_text(data, len) };
}

// ---------------------------------------------------------------------------
// File-level classification
// ---------------------------------------------------------------------------

static std::string classify_file(const std::string& path, const FileOptions* opts) {
    struct stat st;
    int rc = opts->dereference ? stat(path.c_str(), &st) : lstat(path.c_str(), &st);
    if (rc != 0) {
        return "";
    }

    if (S_ISDIR(st.st_mode)) {
        return "directory";
    }
    if (S_ISLNK(st.st_mode)) {
        return "symbolic link";
    }
    if (S_ISREG(st.st_mode)) {
        if (st.st_size == 0) {
            return "empty";
        }

        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            return "";
        }
        uint8_t buf[FILE_READ_BUF];
        ssize_t n = read(fd, buf, sizeof(buf));
        close(fd);
        if (n < 0) {
            return "";
        }
        return classify_bytes(buf, static_cast<size_t>(n)).description;
    }

    return "unknown";
}

static std::string classify_stdin() {
    uint8_t buf[FILE_READ_BUF];
    std::vector<uint8_t> all;
    while (all.size() < FILE_READ_BUF) {
        size_t left = FILE_READ_BUF - all.size();
        ssize_t n = read(0, buf, left);
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) {
            all.push_back(buf[i]);
        }
    }
    return classify_bytes(all.data(), all.size()).description;
}

// ---------------------------------------------------------------------------
// Command entry point
// ---------------------------------------------------------------------------

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... FILE...\n", prog);
    printf("Determine file type of FILE (by examining its contents).\n\n");
    printf("  -b, --brief          don't prefix filenames in output\n");
    printf("  -h, --no-symlinks    do not follow symbolic links\n");
    printf("      --help           display this help and exit\n\n");
    printf("With no FILE, or when FILE is -, read standard input.\n");
}

int file_command(int argc, char** argv) {
    FileOptions opts;

    struct arg_lit* brief_opt = arg_lit0("b", "brief",
        "don't prefix filenames in output");
    struct arg_lit* nosymlinks_opt = arg_lit0("h", "no-symlinks",
        "do not follow symbolic links");
    struct arg_lit* help_opt = arg_lit0(NULL, "help",
        "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100,
        "file to examine (- for stdin)");
    struct arg_end* end = arg_end(20);

    ArgTable at({brief_opt, nosymlinks_opt, help_opt, file_arg, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_help(argv[0]);
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    opts.brief = (brief_opt->count > 0);
    opts.dereference = (nosymlinks_opt->count == 0);

    int exit_status = 0;
    int nfiles = file_arg->count;

    for (int i = 0; i < nfiles; i++) {
        const char* path = file_arg->filename[i];
        std::string desc;
        if (strcmp(path, "-") == 0) {
            desc = classify_stdin();
        } else {
            desc = classify_file(path, &opts);
        }

        if (desc.empty()) {
            fprintf(stderr, "file: '%s': %s\n", path, strerror(errno));
            exit_status = 1;
            continue;
        }

        if (opts.brief) {
            printf("%s\n", desc.c_str());
        } else {
            printf("%s: %s\n", path, desc.c_str());
        }
    }

    if (nfiles == 0) {
        printf("%s\n", classify_stdin().c_str());
    }

    return exit_status;
}

}  // namespace

REGISTER_COMMAND("file", file_command, "Determine file type");
