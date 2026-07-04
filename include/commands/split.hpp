#ifndef SPLIT_HPP
#define SPLIT_HPP

#include <cstdint>

struct SplitOptions {
    int64_t lines = 1000;          // -l / --lines (default 1000, 0 = not set)
    int64_t bytes = 0;             // -b / --bytes (0 = not set)
    int64_t line_bytes = 0;        // -C / --line-bytes (0 = not set)
    int numeric_suffixes = 0;      // -d / --numeric-suffixes
    int hex_suffixes = 0;          // -x / --hex-suffixes
    int suffix_length = 2;         // -a / --suffix-length (default 2)
    int verbose = 0;               // --verbose
    int elide_empty = 0;           // -e / --elide-empty-files
    const char* additional_suffix = nullptr; // --additional-suffix
    const char* filter = nullptr;  // --filter
    const char* separator = nullptr; // -t / --separator
    const char* chunks = nullptr;  // -n / --number
    int unbuffered = 0;            // -u / --unbuffered
    const char* prefix = nullptr;  // PREFIX argument (default "x")
};

void split_command(int argc, char** argv);

#endif
