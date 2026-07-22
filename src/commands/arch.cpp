#include <cstdio>
#include <cstring>
#include <sys/utsname.h>

#include "commands/arch.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Print machine architecture.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

void arch_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("arch (modbox) 1.0\n");
            return;
        }
    }

    struct utsname u;
    if (uname(&u) == 0) {
        printf("%s\n", u.machine);
    }
}

REGISTER_COMMAND("arch", arch_command, "Print machine architecture");
