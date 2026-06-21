#include <glib.h>

#include "commands/lsc.h"
#include "commands/ls.h"

void lsc_command(gint argc, gchar** argv) {
    gint new_argc = argc + 2;
    gchar** new_argv = (gchar**)g_malloc((gulong)(new_argc + 1) * sizeof(gchar*));

    new_argv[0] = argv[0];
    new_argv[1] = "--colorful";
    new_argv[2] = "--icons";

    for (gint i = 1; i < argc; i++) {
        new_argv[i + 2] = argv[i];
    }
    new_argv[new_argc] = NULL;

    ls_command(new_argc, new_argv);

    g_free(new_argv);
}
