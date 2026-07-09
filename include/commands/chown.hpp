#ifndef CHOWN_HPP
#define CHOWN_HPP

#include <sys/types.h>

struct ChownOptions {
    int is_recursive = 0;
    int is_verbose = 0;
    int is_changes = 0;
    int is_silent = 0;
    int preserve_root = 0;
    int no_dereference = 0;
    int traverse_mode = 0;
    int owner_set = 0;
    int group_set = 0;
    uid_t owner = (uid_t)-1;
    gid_t group = (gid_t)-1;
    const char* reference = nullptr;
    int has_from = 0;
    uid_t from_owner = (uid_t)-1;
    gid_t from_group = (gid_t)-1;
};

void chown_command(int argc, char** argv);

#endif
