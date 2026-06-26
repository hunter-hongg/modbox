#ifndef NL_H
#define NL_H

#include <glib.h>

typedef struct {
    char* body_numbering;     /* a, t, n, or pBRE */
    char* header_numbering;
    char* footer_numbering;
    char* number_format;      /* ln, rn, rz */
    char* number_separator;
    char section_delimiters[3]; /* two chars for delimiter */
    int line_increment;
    int join_blank_lines;
    int no_renumber;
    int starting_line_number;
    int number_width;
} NlOptions;

void nl_command(gint argc, gchar** argv);

#endif
