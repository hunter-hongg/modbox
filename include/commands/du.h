#ifndef DU_H
#define DU_H

#include <glib.h>

typedef struct {
    int bytes;              // -b
    int block_size_k;       // -k
    int block_size_m;       // -m
    int human_readable;     // -h
    int summarize;          // -s
    int total;              // -c
    int all;                // -a
    int max_depth;          // -d (-1 = no limit)
    int one_file_system;    // -x
    int count_links;        // -l
    int si;                 // --si (1000, not 1024)
    int apparent_size;      // --apparent-size
    int show_time;          // --time
    int separate_dirs;      // -S
    int null_terminated;    // -0
    char **exclude;         // --exclude patterns
    int exclude_count;
    guint64 threshold;      // -t / --threshold
    int threshold_set;
    // computed scaling factor
    guint64 scale;          // display unit in bytes
} DuOptions;

void du_command(gint argc, gchar **argv);

#endif
