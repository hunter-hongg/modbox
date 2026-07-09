#ifndef CHGRP_HPP
#define CHGRP_HPP

#include <sys/types.h>

struct ChgrpOptions {
    int is_recursive = 0;
    int is_verbose = 0;
    int is_changes = 0;
    int is_silent = 0;
    int preserve_root = 0;
    int no_dereference = 0;
    int traverse_mode = 0;
    int group_set = 0;
    gid_t group = (gid_t)-1;
    const char* reference = nullptr;
};

void chgrp_command(int argc, char** argv);

#endif
