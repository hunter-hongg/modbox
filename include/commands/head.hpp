#ifndef HEAD_HPP
#define HEAD_HPP

#include <cstdint>

struct HeadOptions {
    int64_t lines = 0;        // -n / --lines (0 = use default)
    int64_t bytes = 0;        // -c / --bytes (0 = don't use)
    int quiet = 0;            // -q / --quiet / -s / --silent
    int verbose = 0;          // -v / --verbose
    int zero_terminated = 0;  // -z / --zero-terminated
};

void head_command(int argc, char** argv);

#endif
