#ifndef TAIL_H
#define TAIL_H

#include <glib.h>

typedef struct {
    gint64 lines;         // -n / --lines (0 = use default)
    gint64 bytes;         // -c / --bytes (0 = don't use)
    int follow;           // -f / --follow
    int follow_retry;     // -F (follow + retry on rename)
    int quiet;            // -q / --quiet / -s / --silent
    int verbose;          // -v / --verbose
    int zero_terminated;  // -z / --zero-terminated
    int sleep_interval;   // -s / --sleep-interval (seconds, default 1)
    int is_relative;      // +N mode: show from line/byte N to end
} TailOptions;

void tail_command(gint argc, gchar **argv);

#endif
