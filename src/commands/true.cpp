#include <cstdio>
#include <cstring>

#include "commands/true.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [ignored command line arguments]\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Do nothing, successfully.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int true_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("true");
            return 0;
        }
    }
    // Do nothing, exit with 0
    return 0;
}

REGISTER_COMMAND("true", true_command, "Return true");
