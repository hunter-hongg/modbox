#ifndef TAC_H
#define TAC_H

#include <glib.h>

typedef struct {
    int before_mode;
    int regex_mode;
    const char* separator;
} TacOptions;

void tac_command(gint argc, gchar** argv);

#endif
