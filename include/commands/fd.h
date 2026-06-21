#ifndef FD_H
#define FD_H

#include <glib.h>

typedef enum {
    FD_COLOR_NEVER,
    FD_COLOR_ALWAYS,
    FD_COLOR_AUTO
} fd_color_t;

typedef struct {
    int hidden;
    int no_ignore;
    int case_sensitive;
    int ignore_case;
    int smart_case;
    int glob_mode;
    int full_path;
    int follow;
    int print0;
    int max_depth;
    int max_results;
    fd_color_t color_mode;
    char type_filter;
    gchar *pattern;
    GPtrArray *extensions;
    GPtrArray *exclude;
    GPtrArray *exclude_specs;   /* pre-compiled GPatternSpec* for exclude */
    int has_exec;
    int exec_batch;
    GPtrArray *exec_args;
    GPtrArray *exec_paths;
} FdOptions;

void fd_command(gint argc, gchar **argv);

#endif
