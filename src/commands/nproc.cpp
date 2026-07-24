#include <cstdio>
#include <cstring>
#include <climits>
#include <unistd.h>

#include "commands/nproc.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]...\n", prog);
    printf("Print the number of available processors.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

void nproc_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("nproc (modbox) 1.0\n");
            return;
        }
    }

    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    printf("%ld\n", n);
}

REGISTER_COMMAND("nproc", nproc_command, "Print the number of available processors");
