#include <cstdio>
#include <cstring>

#include "commands/true.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [ignored command line arguments]\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Do nothing, successfully.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

void true_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("true (modbox) 1.0\n");
            return;
        }
    }
    // Do nothing, exit with 0
}

REGISTER_COMMAND("true", true_command, "Return true");
