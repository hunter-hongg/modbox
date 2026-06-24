#ifndef TOUCH_H
#define TOUCH_H

#include <glib.h>

typedef struct {
    int only_atime;
    int only_mtime;
    int no_create;
    const char *reference;
    const char *timestamp;
} TouchOptions;

void touch_command(gint argc, gchar** argv);

#endif
