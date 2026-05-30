#ifndef CAT_H
#define CAT_H

#include <glib.h>

typedef struct {
    int show_line_numbers;
    int show_nonempty_line_numbers;
    int show_ends;
    int squeeze_blank;
    int show_tabs;
    int show_nonprinting;
    int less_mode;

    int blame_mode;
    int highlight_mode;
    int header_mode;
    char* diff_file;

    int range_start;
    int range_end;
    char* grep_pattern;
    int context_lines;
    int head_lines;
    int tail_lines;
    int show_stats;
    int number_format;
} CatOptions;

void cat_command(gint argc, gchar** argv);

#endif
