#include <stdio.h>
#include <glib.h>
#include <string.h>

#include "commands/help.h"

void output_help(gchar* argv0, gchar* progname) {
    if (strcmp(progname, "modbox") == 0) {
        printf("Usage: %s [command] [options]\n", argv0);
        printf("Run \"%s help\" for more information.\n", argv0);
    } else {
        printf("Usage: %s [options]\n", argv0);
    }
}

void help_command(gint argc, gchar** argv) {
    gchar* runname = g_path_get_basename(argv[0]);
    output_help(argv[0], runname);
    cleanup:
    g_free(runname);
}