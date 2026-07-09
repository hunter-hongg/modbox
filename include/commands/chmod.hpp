#ifndef CHMOD_HPP
#define CHMOD_HPP

#include <sys/stat.h>

struct ChmodOptions {
    int is_recursive = 0;
    int is_verbose = 0;
    int is_changes = 0;
    int is_silent = 0;
    int preserve_root = 0;
    mode_t mode = 0;
    int mode_set = 0;       /* 1 if --reference or an explicit mode was given */
    const char* reference = nullptr;
};

void chmod_command(int argc, char** argv);

#endif
