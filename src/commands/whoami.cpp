#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <argtable3.h>
#include "commands/whoami.hpp"
#include "commands/command_macros.hpp"
#include "commands/arg_util.hpp"

int whoami_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, end});
    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Print the user name associated with the current effective user ID.\n");
        printf("\n");
        printf("  -h, --help    display this help and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    uid_t uid = geteuid();
    struct passwd* pw = getpwuid(uid);
    if (pw == NULL) {
        fprintf(stderr, "whoami: cannot find name for user ID %u\n", (unsigned)uid);
        return 0;
    }

    printf("%s\n", pw->pw_name);
    return 0;
}

REGISTER_COMMAND("whoami", whoami_command, "Print effective user name");
