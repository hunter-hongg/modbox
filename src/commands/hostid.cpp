#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "commands/hostid.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print the numeric identifier (in hexadecimal) for the current host.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int hostid_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("hostid");
            return 0;
        }
    }

    long id = gethostid();
    printf("%08lx\n", id);
    return 0;
}

REGISTER_COMMAND("hostid", hostid_command, "Print the numeric identifier (in hexadecimal) for the current host");
