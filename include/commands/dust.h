#ifndef DUST_H
#define DUST_H

#include <glib.h>

typedef struct {
    int max_depth;          // -d / --depth (-1 = no limit)
    int max_lines;          // -n / --number-of-lines (0 = no limit)
    int show_all;           // -a / --all
    int one_file_system;    // -x / --one-file-system
    int si;                 // -H / --si (1000 not 1024)
    int bytes;              // -b / --bytes
    int no_color;           // -c / --no-color
    char **exclude;         // --exclude / -X patterns
    int exclude_count;
} DustOptions;

void dust_command(gint argc, gchar **argv);

#endif
