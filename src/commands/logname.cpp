#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "commands/logname.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print the name of the current user.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

void logname_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("logname (modbox) 1.0\n");
            return;
        }
    }

    const char* name = getlogin();
    if (name == NULL || name[0] == '\0') {
        name = getenv("LOGNAME");
    }
    if (name != NULL && name[0] != '\0') {
        printf("%s\n", name);
    }
}

REGISTER_COMMAND("logname", logname_command, "Print the name of the current user");
