#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "commands/help.h"
#include "commands/cat.h"
#include "commands/ls.h"
#include "commands/lsc.h"
#include "commands/cp.h"
#include "commands/ln.h"
#include "commands/mv.h"
#include "commands/grep.h"
#include "commands/rg.h"
#include "commands/find.h"
#include "commands/fd.h"

typedef void (*command_t)(gint argc, gchar** argv);

static void execute_command(gchar* command, gint argc, gchar** argv) {
    GHashTable* commands = g_hash_table_new(g_str_hash, g_str_equal);

    /* GLib stores values as gpointer (void*). Since POSIX guarantees
     * that function pointers and void* have the same representation,
     * these casts are safe despite being technically non-portable to
     * exotic architectures (e.g. Harvard).
     *
     * Alternatives considered:
     * - Wrapping each command pointer in a struct allocated on the heap:
     *   avoids the cast but adds allocation overhead and complicates cleanup.
     * - Using a separate hash table per command or an if-else chain:
     *   defeats the purpose of dynamic dispatch.
     * - Disabling -Wpedantic globally: too broad a suppression.
     * The current approach is the pragmatic trade-off: isolated warning
     * suppression with a clear rationale. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    g_hash_table_insert(commands, "help", (gpointer)help_command);
    g_hash_table_insert(commands, "cat", (gpointer)cat_command);
    g_hash_table_insert(commands, "ls", (gpointer)ls_command);
    g_hash_table_insert(commands, "cp", (gpointer)cp_command);
    g_hash_table_insert(commands, "ln", (gpointer)ln_command);
    g_hash_table_insert(commands, "mv", (gpointer)mv_command);
    g_hash_table_insert(commands, "grep", (gpointer)grep_command);
    g_hash_table_insert(commands, "find", (gpointer)find_command);
    g_hash_table_insert(commands, "rg", (gpointer)rg_command);
    g_hash_table_insert(commands, "lsc", (gpointer)lsc_command);
    g_hash_table_insert(commands, "fd", (gpointer)fd_command);

    if (g_hash_table_contains(commands, command)) {
        command_t cmd = (command_t)(void*)g_hash_table_lookup(commands, command);
#pragma GCC diagnostic pop
        cmd(argc, argv);
    } else {
        gchar* runname = g_path_get_basename(argv[0]);
        printf("Unknown command: %s\n", command);
        output_help(argv[0], runname);
    }

    g_hash_table_destroy(commands);
}

int main(int argc, char* argv[])
{
    gchar* runname = g_path_get_basename(argv[0]);
    if ((strcmp(runname, "modbox") == 0) && (argc == 1)) {
        output_help(argv[0], runname);
        goto cleanup;
    }
    if (strcmp(runname, "modbox") == 0) {
        execute_command(argv[1], argc - 1, argv + 1);
    } else {
        execute_command(runname, argc, argv);
    }
    cleanup:
    g_free(runname);
    return 0;
}
