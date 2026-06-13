#ifndef MV_H
#define MV_H

#include <glib.h>

typedef struct {
    int is_interactive;
    int is_no_clobber;
} MvOptions;

void mv_command(gint argc, gchar** argv);

#endif

