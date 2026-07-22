#include "commands/fs_classify.hpp"

#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

const char* const SUFFIXES[] = { "", "K", "M", "G", "T", "P", "E" };

}

FileType classify(const struct stat& st) {
    if (S_ISDIR(st.st_mode)) return FileType::Directory;
    if (S_ISLNK(st.st_mode)) return FileType::Symlink;
    if (S_ISSOCK(st.st_mode)) return FileType::Socket;
    if (S_ISFIFO(st.st_mode)) return FileType::Fifo;
    if (S_ISBLK(st.st_mode)) return FileType::BlockDev;
    if (S_ISCHR(st.st_mode)) return FileType::CharDev;
    if (S_ISREG(st.st_mode)) {
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            return FileType::Executable;
        return FileType::Regular;
    }
    return FileType::Unknown;
}

std::string format_bytes(uint64_t bytes) {
    if (bytes == 0) return "0";

    int level = 0;
    double value = (double)bytes;
    while (value >= 1024.0 && level < 6) {
        value /= 1024.0;
        level++;
    }

    char buf[64];
    if (level == 0) {
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)bytes);
    } else if (value >= 100.0) {
        snprintf(buf, sizeof(buf), "%.0f%s", value, SUFFIXES[level]);
    } else if (value >= 10.0) {
        snprintf(buf, sizeof(buf), "%.1f%s", value, SUFFIXES[level]);
    } else {
        snprintf(buf, sizeof(buf), "%.1f%s", value, SUFFIXES[level]);
    }
    return buf;
}
