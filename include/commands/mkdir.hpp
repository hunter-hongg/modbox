#ifndef MKDIR_HPP
#define MKDIR_HPP

#include <sys/stat.h>

struct MkdirOptions {
    int is_parents = 0;
    int is_verbose = 0;
    mode_t mode = 0755;
};

void mkdir_command(int argc, char** argv);

#endif
