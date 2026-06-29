#ifndef TAIL_HPP
#define TAIL_HPP

#include <cstdint>

struct TailOptions {
    int64_t lines = 0;         // -n / --lines (0 = use default)
    int64_t bytes = 0;         // -c / --bytes (0 = don't use)
    int follow = 0;            // -f / --follow
    int follow_retry = 0;      // -F (follow + retry on rename)
    int quiet = 0;             // -q / --quiet / -s / --silent
    int verbose = 0;           // -v / --verbose
    int zero_terminated = 0;   // -z / --zero-terminated
    int sleep_interval = 1;    // -s / --sleep-interval
    int is_relative = 0;       // +N mode: show from line/byte N to end
};

void tail_command(int argc, char** argv);

#endif
