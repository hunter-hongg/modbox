#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <set>
#include <utmp.h>
#include <argtable3.h>

#include "commands/users.hpp"
#include "commands/arg_util.hpp"
#include "commands/utmp_util.hpp"
#include "commands/command_macros.hpp"

int users_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_opt = arg_filen(NULL, NULL, "FILE", 0, 1, "use FILE instead of /var/run/utmp");
    struct arg_end* end = arg_end(20);
    ArgTable at({help_opt, file_opt, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]\n", argv[0]);
        printf("Print the user names of users currently logged in.\n");
        printf("\n");
        printf("  -h, --help  display this help and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    const char* utmp_path = "/var/run/utmp";
    if (file_opt->count > 0) {
        utmp_path = file_opt->filename[0];
    }

    utmpname(utmp_path);
    std::set<std::string> names;

    for_each_utmp_user([&names](const struct utmp& u) {
        names.insert(u.ut_user);
    });

    bool first = true;
    for (const auto& name : names) {
        if (!first) printf(" ");
        printf("%s", name.c_str());
        first = false;
    }
    printf("\n");

    return 0;
}

REGISTER_COMMAND("users", users_command, "Print user names of users currently logged in");