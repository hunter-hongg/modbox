#include <argtable3.h>
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
    ELF32,
    ELF64,
    GZIP,
    ZIP,
    SYMBOLIC_LINK,
    DIRECTORY,
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
        { kElfMagic,   4, 0, FileType::ELF32,     nullptr },
        { kGzipMagic,  2, 0, FileType::GZIP,      "gzip compressed data" },
        { kZipMagic,   4, 0, FileType::ZIP,       "ZIP archive data" },
        { kUtf8Bom,    3, 0, FileType::UTF8_TEXT, nullptr },
        { kShebang,    2, 0, FileType::SHELL_SCRIPT, nullptr },
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
            i++; // skip \n
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
    if (len == 0) {
        return true;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = data[i];
        if (c == 0x00) return false;
        if (c == 0x09 || c == 0x0a || c == 0x0d || (c >= 0x20 && c <= 0x7e)) {
            continue;
        }
        // High bytes (> 0x7e) disqualify plain ASCII text.
        if (c < 0x80) return false;
    }
    return true;
}

static bool is_utf8_text(const uint8_t* data, size_t len) {
    // Accept any file that is not pure ASCII and contains no null bytes,
    // treating it as UTF-8 Unicode text (matches GNU file behaviour for
    // typical inputs).
    if (len == 0) return true;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == 0x00) return false;
    }
    return true;
}

static std::string classify_text(const uint8_t* data, size_t len,
                                  bool has_shebang) {
    bool is_ascii = is_ascii_text(data, len);

    std::string desc;
    if (is_ascii) {
        desc = "ASCII text";
    } else if (has_shebang) {
        desc = "UTF-8 Unicode text";
    } else {
        desc = "UTF-8 Unicode text";
    }

    std::string le = detect_line_endings(data, len);
    if (!le.empty()) {
        desc += le;
    }

    if (has_shebang && len > 2) {
        // Extract interpreter from shebang line — stop at first newline
        size_t end = len;
        for (size_t j = 2; j < len; j++) {
            if (data[j] == '\n' || data[j] == '\r') {
                end = j;
                break;
            }
        }
        std::string line(reinterpret_cast<const char*>(data + 2), end - 2);
        // Trim trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        // If it starts with "env ", show "env <prog>"
        if (line.rfind("env", 0) == 0) {
            // Skip "env " and take the next token
            size_t start = line.find_first_not_of(' ');
            if (start != std::string::npos) {
                size_t sp = line.find_first_of(' ', start);
                std::string prog = (sp == std::string::npos)
                                      ? line.substr(start)
                                      : line.substr(start, sp - start);
                desc += ", with " + prog;
            } else {
                desc += ", with env";
            }
        } else if (!line.empty()) {
            desc += ", with " + line;
        }
    }
    return desc;
}

// ---------------------------------------------------------------------------
// ELF classification
// ---------------------------------------------------------------------------

static std::string classify_elf(const uint8_t* data, size_t len, int class_byte) {
    std::string desc = (class_byte == 1) ? "ELF 32-bit" : "ELF 64-bit";

    if (len >= 18) {
        uint16_t machine = static_cast<uint16_t>(data[18]) |
                           (static_cast<uint16_t>(data[19]) << 8);
        uint16_t type = static_cast<uint16_t>(data[16]) |
                        (static_cast<uint16_t>(data[17]) << 8);
        const char* endianness =
            (data[5] == 1) ? "little-endian" : "big-endian";
        desc += " LSB " + std::string(endianness) + " executable";
        if (type == 2) {
            desc += " (shared object)";
        } else if (type == 3) {
            desc += " (dynamically linked)";
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
// Core classifier: read up to 4096 bytes and classify
// ---------------------------------------------------------------------------

static const size_t FILE_READ_BUF = 4096;

struct ClassifyResult {
    FileType type;
    std::string description;
};

ClassifyResult classify_bytes(const uint8_t* data, size_t len) {
    // Empty file
    if (len == 0) {
        return { FileType::EMPTY, "empty" };
    }

    // Check magic entries
    for (const auto& entry : magic_table()) {
        if (entry.type == FileType::ELF32) {
            // ELF needs the class byte
            if (matches_pattern(data, len, entry) && len >= 5) {
                std::string desc = classify_elf(data, len, data[4]);
                return { FileType::ELF32, desc };
            }
        }
        if (entry.type == FileType::UTF8_TEXT ||
            entry.type == FileType::SHELL_SCRIPT) {
            continue; // handle these after binary magic
        }
        if (matches_pattern(data, len, entry)) {
            return { entry.type, entry.description };
        }
    }

    // UTF-8 BOM text
    if (matches_pattern(data, len, { kUtf8Bom, 3, 0, FileType::UTF8_TEXT, nullptr })) {
        bool has_shebang = (len > 5) && matches_pattern(data, len,
                                  { kShebang, 2, 3, FileType::SHELL_SCRIPT, nullptr });
        return { FileType::UTF8_TEXT, classify_text(data, len, has_shebang) };
    }

    // Shebang detection
    bool has_shebang = matches_pattern(data, len, { kShebang, 2, 0, FileType::SHELL_SCRIPT, nullptr });

    if (has_shebang) {
        return { FileType::SHELL_SCRIPT, classify_text(data, len, true) };
    }

    // Text: ASCII or UTF-8
    if (is_ascii_text(data, len)) {
        return { FileType::ASCII_TEXT, "ASCII text" };
    }
    if (is_utf8_text(data, len)) {
        return { FileType::UTF8_TEXT, "UTF-8 Unicode text" };
    }

    return { FileType::UNKNOWN_DATA, "data" };
}

// ---------------------------------------------------------------------------
// File-level classification
// ---------------------------------------------------------------------------

static std::string classify_file(const std::string& path, bool deref) {
    // First stat (with or without following symlinks)
    struct stat st;
    int rc;
    if (deref) {
        rc = stat(path.c_str(), &st);
    } else {
        rc = lstat(path.c_str(), &st);
    }
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
        size_t len = static_cast<size_t>(n);
        ClassifyResult result = classify_bytes(buf, len);
        return result.description;
    }

    return "unknown";
}

static std::string classify_stdin() {
    uint8_t buf[FILE_READ_BUF];
    std::vector<uint8_t> all;
    while (true) {
        size_t left = FILE_READ_BUF - (all.size() < FILE_READ_BUF ? all.size() : FILE_READ_BUF);
        if (left == 0) break;
        ssize_t n = read(0, buf, left);
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) {
            all.push_back(buf[i]);
            if (all.size() >= FILE_READ_BUF) break;
        }
        if (n < (ssize_t)left) break;
    }
    ClassifyResult result = classify_bytes(all.data(), all.size());
    return result.description;
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

    // Parse options manually (keeps it lean; argtable3 is also fine)
    std::vector<std::string> operands;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "-b") == 0 || strcmp(arg, "--brief") == 0) {
            opts.brief = true;
        } else if (strcmp(arg, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--no-symlinks") == 0) {
            opts.no_symlinks = true;
            opts.dereference = false;
        } else if (strcmp(arg, "--no-symlinks") == 0) {
            opts.no_symlinks = true;
            opts.dereference = false;
        } else if (strcmp(arg, "-") == 0) {
            // '-' is stdin, valid operand
            operands.push_back(arg);
        } else if (arg[0] == '-') {
            fprintf(stderr, "file: invalid option '%s'\n", arg);
            return 1;
        } else {
            operands.push_back(arg);
        }
    }

    int exit_status = 0;

    if (operands.empty()) {
        // Read from stdin
        std::string desc = classify_stdin();
        printf("%s\n", desc.c_str());
        return 0;
    }

    for (const auto& path : operands) {
        std::string desc;
        if (path == "-") {
            desc = classify_stdin();
        } else {
            desc = classify_file(path, opts.dereference);
        }

        if (desc.empty()) {
            fprintf(stderr, "file: '%s': %s\n", path.c_str(), strerror(errno));
            exit_status = 1;
            continue;
        }

        if (opts.brief) {
            printf("%s\n", desc.c_str());
        } else {
            printf("%s: %s\n", path.c_str(), desc.c_str());
        }
    }

    return exit_status;
}

}  // namespace

REGISTER_COMMAND("file", file_command, "Determine file type");
