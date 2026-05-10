#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "commands/help.h"
#include "commands/cat.h"
#include "commands/ls.h"
#include "commands/cp.h"
#include "commands/ln.h"
#include "commands/mv.h"

typedef void (*command_t)(gint argc, gchar** argv);

static void execute_command(gchar* command, gint argc, gchar** argv) {
    GHashTable* commands = g_hash_table_new(g_str_hash, g_str_equal);

    g_hash_table_insert(commands, "help", help_command);
    g_hash_table_insert(commands, "cat", cat_command);
    g_hash_table_insert(commands, "ls", ls_command);
    g_hash_table_insert(commands, "cp", cp_command);
    g_hash_table_insert(commands, "ln", ln_command);
    g_hash_table_insert(commands, "mv", mv_command);

    if (g_hash_table_contains(commands, command)) {
        command_t cmd = g_hash_table_lookup(commands, command);
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
