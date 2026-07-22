#ifndef FS_CLASSIFY_HPP
#define FS_CLASSIFY_HPP

#include <string>
#include <cstdint>
#include <sys/stat.h>

enum class FileType : uint8_t {
    Unknown = 0,
    Regular = 1,
    Executable = 2,
    Directory = 3,
    Symlink = 4,
    Socket = 5,
    Fifo = 6,
    BlockDev = 7,
    CharDev = 8
};

FileType classify(const struct stat& st);

inline char type_prefix_char(FileType ft) {
    switch (ft) {
    case FileType::Directory: return 'd';
    case FileType::Symlink: return 'l';
    case FileType::Socket: return 's';
    case FileType::Fifo: return 'p';
    case FileType::BlockDev: return 'b';
    case FileType::CharDev: return 'c';
    default: return '-';
    }
}

inline const char* type_prefix_char_str(FileType ft) {
    switch (ft) {
    case FileType::Directory: return "d";
    case FileType::Symlink: return "l";
    case FileType::Socket: return "s";
    case FileType::Fifo: return "p";
    case FileType::BlockDev: return "b";
    case FileType::CharDev: return "c";
    default: return "-";
    }
}

inline const char* icon_utf8(FileType ft) {
    switch (ft) {
    case FileType::Directory: return "\xEF\x84\x95";
    case FileType::Symlink: return "\xEF\x95\xB0";
    case FileType::Socket: return "\xEF\x94\xA1";
    case FileType::Fifo: return "\xEF\x94\xA2";
    case FileType::BlockDev: case FileType::CharDev: return "\xEF\xA4\x81";
    case FileType::Executable: return "\xEF\x92\x89";
    default: return "\xEF\x80\x96";
    }
}

inline const char* classify_suffix(FileType ft) {
    switch (ft) {
    case FileType::Directory: return "/";
    case FileType::Symlink: return "@";
    case FileType::Socket: return "=";
    case FileType::Fifo: return "|";
    case FileType::Executable: return "*";
    default: return "";
    }
}

inline const char* ansi_color_code(FileType ft) {
    switch (ft) {
    case FileType::Directory: return "\033[1;34m";
    case FileType::Symlink: return "\033[1;36m";
    case FileType::Socket: return "\033[1;35m";
    case FileType::Fifo: return "\033[1;33m";
    case FileType::BlockDev: case FileType::CharDev: return "\033[1;33m";
    case FileType::Executable: return "\033[1;32m";
    default: return "\033[0m";
    }
}

inline const char* human_label(FileType ft) {
    switch (ft) {
    case FileType::Directory: return "directory";
    case FileType::Symlink: return "symlink";
    case FileType::Socket: return "socket";
    case FileType::Fifo: return "fifo";
    case FileType::BlockDev: return "block device";
    case FileType::CharDev: return "character device";
    case FileType::Executable: return "executable";
    case FileType::Regular: return "regular file";
    default: return "unknown";
    }
}

std::string format_bytes(uint64_t bytes);

#endif
