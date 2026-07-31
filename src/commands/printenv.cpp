#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#include "commands/printenv.hpp"
#include "commands/command_macros.hpp"

extern char** environ;

int printenv_command(int argc, char** argv) {
    bool null_out = false;
    bool help = false;
    std::vector<std::string> vars;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-0") == 0 || strcmp(a, "--null") == 0) {
            null_out = true;
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            help = true;
        } else {
            vars.push_back(a);
        }
    }

    if (help) {
        printf("Usage: %s [OPTION]... [NAME]...\n", argv[0]);
        printf("Print environment variables.\n");
        printf("\n");
        printf("  -0, --null     end each output line with NUL, not newline\n");
        printf("  -h, --help     display this help and exit\n");
        return 0;
    }

    if (vars.empty()) {
        for (char** e = environ; *e != nullptr; e++) {
            fputs(*e, stdout);
            fputc(null_out ? '\0' : '\n', stdout);
        }
        return 0;
    }

    int found_count = 0;
    for (const auto& name : vars) {
        const char* val = getenv(name.c_str());
        if (val) {
            if (null_out) {
                printf("%s=%s%c", name.c_str(), val, '\0');
            } else {
                printf("%s=%s\n", name.c_str(), val);
            }
            found_count++;
        }
    }

    if (found_count < (int)vars.size()) {
        exit(1);
    }
    return 0;
}
REGISTER_COMMAND("printenv", printenv_command, "Print environment variables");