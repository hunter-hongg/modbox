#ifndef RM_H
#define RM_H

#include <glib.h>

typedef struct {
    int is_recursive;
    int is_force;
    int is_interactive;
    int is_verbose;
    int remove_empty_dirs;
} RmOptions;

void rm_command(gint argc, gchar** argv);

#endif
