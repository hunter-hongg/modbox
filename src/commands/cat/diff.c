#include <stdio.h>
#include <glib.h>

#include "commands/cat/diff.h"

void run_diff(const char* file1, const char* file2, FILE* out) {
    gchar *stdout_buf = NULL;
    GError *error = NULL;

    gchar *argv[] = {(gchar*)"diff", (gchar*)"-u", (gchar*)file1, (gchar*)file2, NULL};

    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, &stdout_buf, NULL, NULL, &error)) {
        if (error) { g_error_free(error); }
        return;
    }

    if (stdout_buf) {
        (void)fputs(stdout_buf, out);
        g_free(stdout_buf);
    }
}
