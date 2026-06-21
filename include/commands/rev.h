#ifndef REV_H
#define REV_H

#include <glib.h>

typedef struct {
    int dummy;  // placeholder for potential future options
} RevOptions;

void rev_command(gint argc, gchar **argv);

#endif
