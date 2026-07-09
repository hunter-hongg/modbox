#include <cstdio>
#include <cstring>
#include "commands/help.hpp"

void output_help(const char* argv0, const char* progname) {
    if (strcmp(progname, "modbox") == 0) {
        printf("Usage: %s <command> [options]\n", argv0);
        printf("\n");
        printf("Available commands:\n");
        printf("  cat    Concatenate files and print on the standard output\n");
        printf("  chmod  Change file mode bits\n");
        printf("  chgrp  Change group ownership\n");
        printf("  chown  Change file owner and group\n");
        printf("  comm   Compare two sorted files line by line\n");
        printf("  cp     Copy files and directories\n");
        printf("  diff   Compare files line by line\n");
        printf("  du     Estimate file space usage\n");
        printf("  dust   Alias for du --max-depth=1 -h\n");
        printf("  fd     Alias for find --color\n");
        printf("  find   Search for files in a directory hierarchy\n");
        printf("  grep   Search for patterns in files\n");
        printf("  head   Output the first part of files\n");
        printf("  help   Display this help message\n");
        printf("  htop   Interactive process viewer (htop-style TUI)\n");
        printf("  ln     Create hard/symbolic links between files\n");
        printf("  ls     List directory contents\n");
        printf("  lsc    Alias for ls --colorful --icons\n");
        printf("  mkdir  Create directories\n");
        printf("  mv     Move (rename) files\n");
        printf("  nl     Number lines of files\n");
        printf("  ptx    Generate a permuted index (KWIC index)\n");
        printf("  rev    Reverse lines characterwise\n");
        printf("  rg     Alias for grep --color=always -r\n");
        printf("  rm     Remove files or directories\n");
        printf("  sort   Sort lines of text files\n");
        printf("  tac    Concatenate and print files in reverse\n");
        printf("  tail   Output the last part of files\n");
        printf("  top    Display Linux processes\n");
        printf("  touch  Change file timestamps\n");
        printf("  uname  Print system information\n");
        printf("  uniq   Report or omit repeated lines\n");
        printf("  whoami Print the current effective user name\n");
        printf("  zoxide Smart directory jumping based on frecency\n");
        printf("\n");
        printf("Run \"%s help <command>\" or \"%s <command> --help\" for detailed help on a specific command.\n", argv0, argv0);
    } else {
        printf("Usage: %s [options]\n", argv0);
    }
}

void help_command(int argc, char** argv) {
    (void)argc;
    (void)argv;
    output_help("modbox", "modbox");
}
