#ifndef DU_HPP
#define DU_HPP

#include <cstdint>

struct DuOptions {
    int bytes = 0;               // -b
    int block_size_k = 0;        // -k
    int block_size_m = 0;        // -m
    int human_readable = 0;      // -h
    int summarize = 0;           // -s
    int total = 0;               // -c
    int all = 0;                 // -a
    int max_depth = -1;          // -d (-1 = no limit)
    int one_file_system = 0;     // -x
    int count_links = 0;         // -l
    int si = 0;                  // --si (1000, not 1024)
    int apparent_size = 0;       // --apparent-size
    int show_time = 0;           // --time
    int separate_dirs = 0;       // -S
    int null_terminated = 0;     // -0
    char** exclude = nullptr;    // --exclude patterns
    int exclude_count = 0;
    uint64_t threshold = 0;      // -t / --threshold
    int threshold_set = 0;
    uint64_t scale = 0;          // display unit in bytes
};

void du_command(int argc, char** argv);

#endif
