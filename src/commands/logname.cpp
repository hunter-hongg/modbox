#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "commands/logname.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print the name of the current user.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int logname_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("logname");
            return 0;
        }
    }

    const char* name = getlogin();
    if (name == NULL || name[0] == '\0') {
        name = getenv("LOGNAME");
    }
    if (name != NULL && name[0] != '\0') {
        printf("%s\n", name);
    }
    return 0;
}

REGISTER_COMMAND("logname", logname_command, "Print the name of the current user");
