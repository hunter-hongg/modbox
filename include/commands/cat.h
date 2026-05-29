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
} CatOptions;

void cat_command(gint argc, gchar** argv);

#endif