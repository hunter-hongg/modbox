#ifndef MKDIR_H
#define MKDIR_H

#include <glib.h>
#include <sys/stat.h>

typedef struct {
    int is_parents;
    int is_verbose;
    mode_t mode;
} MkdirOptions;

void mkdir_command(gint argc, gchar** argv);

#endif
