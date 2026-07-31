#include <cstdio>
#include <cstring>
#include <sys/utsname.h>

#include "commands/arch.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print machine architecture.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int arch_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("arch");
            return 0;
        }
    }

    struct utsname u;
    if (uname(&u) == 0) {
        printf("%s\n", u.machine);
    }
    return 0;
}

REGISTER_COMMAND("arch", arch_command, "Print machine architecture");
