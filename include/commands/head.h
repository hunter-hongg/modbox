#ifndef HEAD_H
#define HEAD_H

#include <glib.h>

typedef struct {
    gint64 lines;         // -n / --lines (0 = use default)
    gint64 bytes;         // -c / --bytes (0 = don't use)
    int quiet;            // -q / --quiet / -s / --silent
    int verbose;          // -v / --verbose
    int zero_terminated;  // -z / --zero-terminated
} HeadOptions;

void head_command(gint argc, gchar **argv);

#endif
