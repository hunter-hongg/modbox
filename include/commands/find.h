#ifndef FIND_H
#define FIND_H

#include <glib.h>

typedef struct {
    GPtrArray *paths;           // starting points
    // Predicates
    gchar *name_pattern;        // -name
    gchar *iname_pattern;       // -iname
    char type_filter;           // -type: 'f', 'd', 'l', or 0 (any)
    int empty_only;             // -empty
    // Numeric options
    int max_depth;              // -maxdepth, -1 = unlimited
    int min_depth;              // -mindepth
    // Actions
    int has_action;             // any action specified?
    int do_print;               // -print
    int do_delete;              // -delete
    int has_exec;               // -exec
    GPtrArray *exec_args;       // exec command arguments
    int exec_plus;              // -exec ... + (batch mode)
    GPtrArray *exec_paths;      // accumulated paths for exec +
} FindOptions;

void find_command(gint argc, gchar **argv);

#endif
