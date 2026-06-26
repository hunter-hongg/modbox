#ifndef UNIQ_H
#define UNIQ_H

#include <glib.h>

typedef struct {
    int count;
    int repeated;
    int all_repeated;
    int unique;
    int ignore_case;
    int skip_fields;
    int skip_chars;
    int check_chars;
} UniqOptions;

void uniq_command(gint argc, gchar** argv);

#endif
