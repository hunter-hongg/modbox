#include <cstdio>
#include <cstring>
#include <climits>
#include <unistd.h>

#include "commands/nproc.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]...\n", prog);
    printf("Print the number of available processors.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
}

int nproc_command(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("nproc");
            return 0;
        }
    }

    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    printf("%ld\n", n);
    return 0;
}

REGISTER_COMMAND("nproc", nproc_command, "Print the number of available processors");
