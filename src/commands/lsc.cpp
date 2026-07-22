#include <cstdlib>
#include <cstring>

#include "commands/lsc.hpp"
#include "commands/ls.hpp"
#include "commands/command_macros.hpp"

void lsc_command(int argc, char** argv) {
    int new_argc = argc + 2;
    char** new_argv = (char**)malloc((size_t)(new_argc + 1) * sizeof(char*));

    new_argv[0] = argv[0];
    new_argv[1] = "--colorful";
    new_argv[2] = "--icons";

    for (int i = 1; i < argc; i++) {
        new_argv[i + 2] = argv[i];
    }
    new_argv[new_argc] = nullptr;

    ls_command(new_argc, new_argv);

    free(new_argv);
}

REGISTER_COMMAND("lsc", lsc_command, "Alias for ls --colorful --icons");
