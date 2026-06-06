#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "commands/help.h"

void output_help(gchar *argv0, gchar *progname) {
  if (strcmp(progname, "modbox") == 0) {
    printf("Usage: %s <command> [options]\n", argv0);
    printf("\n");
    printf("Available commands:\n");
    printf("  cat   Concatenate files and print on the standard output\n");
    printf("  cp    Copy files and directories\n");
    printf("  help  Display this help message\n");
    printf("  ln    Create hard links between files\n");
    printf("  ls    List directory contents\n");
    printf("  grep  Search for patterns in files\n");
    printf("  mv    Move (rename) files\n");
    printf("\n");
    printf("Run \"%s help <command>\" or \"%s <command> --help\" for detailed "
           "help on a specific command.\n",
           argv0, argv0);
  } else {
    printf("Usage: %s [options]\n", argv0);
  }
}

void help_command(gint argc, gchar **argv) {
  (void)argc;
  (void)argv;
  // argv[0] is just the command name "help" (from command dispatch),
  // so we can't reconstruct the original binary path.
  // Always show the full modbox command listing.
  output_help("modbox", "modbox");
}
