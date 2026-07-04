#include <cstdlib>
#include <cstring>

#include "commands/dir.hpp"
#include "commands/ls.hpp"

void dir_command(int argc, char** argv) {
    int new_argc = argc + 1;
    char** new_argv = (char**)malloc((size_t)(new_argc + 1) * sizeof(char*));

    new_argv[0] = argv[0];
    new_argv[1] = const_cast<char*>("-C");

    for (int i = 1; i < argc; i++) {
        new_argv[i + 1] = argv[i];
    }
    new_argv[new_argc] = nullptr;

    ls_command(new_argc, new_argv);

    free(new_argv);
}
