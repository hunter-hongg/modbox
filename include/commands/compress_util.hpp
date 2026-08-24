#ifndef COMPRESS_UTIL_HPP
#define COMPRESS_UTIL_HPP

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "commands/cmd_error.hpp"

namespace compress_util {

constexpr size_t READ_BUF_SIZE = 65536;
constexpr double PERCENT_100 = 100.0;

inline bool read_all(FILE* fp, std::vector<unsigned char>& out) {
    unsigned char buf[READ_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, READ_BUF_SIZE, fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return ferror(fp) == 0;
}

inline bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string strip_suffix(const std::string& p, const std::string& suffix) {
    return ends_with(p, suffix) ? p.substr(0, p.size() - suffix.size()) : p;
}

// Write `data` to `outname` unless it exists and !force. Returns 0 on success.
inline int write_output_file(const std::vector<unsigned char>& data,
                             const std::string& outname, bool force,
                             const char* prog) {
    if (!force) {
        struct stat st;
        if (stat(outname.c_str(), &st) == 0) {
            fprintf(stderr, "%s: %s: File exists\n", prog, outname.c_str());
            return 1;
        }
    }
    FILE* out = fopen(outname.c_str(), "wb");
    if (!out) {
        cmd_perror(prog, outname.c_str());
        return 1;
    }
    if (fwrite(data.data(), 1, data.size(), out) != data.size()) {
        fclose(out);
        cmd_perror(prog, outname.c_str());
        return 1;
    }
    int close_res = fclose(out);
    if (close_res != 0) {
        cmd_perror(prog, outname.c_str());
        return 1;
    }
    return 0;
}

inline void print_ratio(const std::string& name, size_t in_size,
                        size_t out_size, const char* replaced_with = nullptr) {
    double ratio = in_size == 0 ? PERCENT_100 : ((1.0 - ((double)out_size / (double)in_size)) * PERCENT_100);
    if (replaced_with != nullptr) {
        printf("%s: %5.1f%% -- replaced with %s\n", name.c_str(), ratio, replaced_with);
    } else {
        printf("%s: %5.1f%%\n", name.c_str(), ratio);
    }
}

} // namespace compress_util

#endif // COMPRESS_UTIL_HPP
