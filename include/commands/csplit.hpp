#ifndef CSPLIT_HPP
#define CSPLIT_HPP

#include <cstdint>

struct CsplitOptions {
    const char* prefix = nullptr;   // -f / --prefix (default "xx")
    const char* suffix_format = nullptr; // -b / --suffix-format
    int digits = 2;                 // -n / --digits (default 2)
    int elide_empty = 0;            // -z / --elide-empty-files
    int quiet = 0;                  // -s / --quiet / --silent
    int keep_files = 0;             // -k / --keep-files
};

void csplit_command(int argc, char** argv);

#endif
