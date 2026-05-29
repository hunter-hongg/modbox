#ifndef LN_H
#define LN_H

#include <glib.h>

typedef struct {
    int is_verbose;
    int is_force;
    int is_sym;
    int is_interactive;
    int is_no_deref;
} LnOptions;

void ln_command(gint argc, gchar** argv);

#endif
