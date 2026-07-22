#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "commands/false.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [ignored command line arguments]\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Do nothing, unsuccessfully.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

void false_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("false (modbox) 1.0\n");
            return;
        }
    }
    exit(1);
}

REGISTER_COMMAND("false", false_command, "Return false");
