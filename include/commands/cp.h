#ifndef CP_H
#define CP_H

#include <glib.h>
#include <sys/stat.h>

typedef struct {
    int is_recursive;
    int is_verbose;
    int is_force;
    int is_no_clobber;
    int is_interactive;
    int is_update;
    int is_preserve;
    const char *target_dir;
    const struct stat *src_stat;
} CpOptions;

void cp_command(gint argc, gchar** argv);

#endif
