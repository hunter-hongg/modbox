#ifndef CP_H
#define CP_H

#include <glib.h>

typedef struct {
    int is_recursive;
    int is_verbose;
    int is_force;
    int is_no_clobber;
} CpOptions;

void cp_command(gint argc, gchar** argv);

#endif
