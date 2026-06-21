#ifndef SORT_H
#define SORT_H

#include <glib.h>

typedef struct {
    int ignore_leading_blanks;   // -b
    int ignore_case;             // -f
    int numeric_sort;            // -n
    int reverse;                 // -r
    int unique;                  // -u
    int check;                   // -c (check + report first bad line)
    int stable;                  // -s (disable last-resort comparison)
    char *key_spec;              // -k POS1[,POS2]
    char field_separator;        // -t SEP (0 = default blank transition)
    char *output_file;           // -o FILE
    // Internal parsed key specs
    GPtrArray *keys;             // array of ParsedKey*
} SortOptions;

void sort_command(gint argc, gchar **argv);

#endif
