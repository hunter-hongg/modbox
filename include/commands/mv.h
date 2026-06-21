#ifndef MV_H
#define MV_H

#include <glib.h>

typedef struct {
    int is_interactive;
    int is_no_clobber;
    int is_force;
    int is_verbose;
    int is_update;
    int is_backup;
    const char *target_dir;
    int no_target_dir;
} MvOptions;

void mv_command(gint argc, gchar** argv);

#endif

