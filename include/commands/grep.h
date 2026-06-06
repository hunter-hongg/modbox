#ifndef GREP_H
#define GREP_H

#include <glib.h>

typedef enum {
    GREP_MODE_BASIC,
    GREP_MODE_EXTENDED,
    GREP_MODE_FIXED
} grep_mode_t;

typedef enum {
    COLOR_NEVER_GREP,
    COLOR_ALWAYS_GREP,
    COLOR_AUTO_GREP
} grep_color_t;

typedef struct {
    int ignore_case;
    int invert_match;
    int line_number;
    int recursive;
    int count_only;
    int word_regexp;
    int files_with_matches;
    int line_regexp;
    int only_matching;
    int always_show_filename;
    int never_show_filename;
    grep_mode_t mode;
    grep_color_t color_mode;
    gchar *pattern;
} GrepOptions;

void grep_command(gint argc, gchar** argv);

#endif
