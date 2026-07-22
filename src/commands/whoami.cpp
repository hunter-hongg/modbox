#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <argtable3.h>
#include "commands/whoami.hpp"
#include "commands/command_macros.hpp"

void whoami_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {help_opt, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Print the user name associated with the current effective user ID.\n");
        printf("\n");
        printf("  -h, --help    display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    uid_t uid = geteuid();
    struct passwd* pw = getpwuid(uid);
    if (pw == NULL) {
        fprintf(stderr, "whoami: cannot find name for user ID %u\n", (unsigned)uid);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    printf("%s\n", pw->pw_name);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("whoami", whoami_command, "Print effective user name");
