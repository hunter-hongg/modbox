#ifndef RG_H
#define RG_H

#include <glib.h>

typedef enum {
    RG_MODE_BASIC,
    RG_MODE_EXTENDED,
    RG_MODE_FIXED
} rg_mode_t;

typedef enum {
    RG_COLOR_NEVER,
    RG_COLOR_ALWAYS,
    RG_COLOR_AUTO
} rg_color_t;

typedef struct RgOptions {
    int line_number;        // -n (default: 1)
    int no_line_number;     // -N (suppress line numbers)
    int ignore_case;        // -i
    int smart_case;         // -S (default: 1 — case-insensitive if pattern is lowercase)
    int case_sensitive;     // -s (force case-sensitive)
    int invert_match;       // -v
    int word_regexp;        // -w
    int line_regexp;        // -x
    int only_matching;      // -o
    int files_with_matches; // -l
    int count_only;         // -c
    int fixed_strings;      // -F
    int hidden;             // --hidden
    int no_ignore;          // --no-ignore (implies --hidden)
    int max_count;          // -m (0 = unlimited)
    int max_depth;          // --max-depth (-1 = unlimited)
    int context_before;     // -B / --before-context
    int context_after;      // -A / --after-context
    int context;            // -C / --context (both sides)
    rg_mode_t mode;
    rg_color_t color_mode;
    gchar *pattern;
    GPtrArray *glob_patterns; // -g / --glob patterns
} RgOptions;

void rg_command(gint argc, gchar** argv);

#endif
