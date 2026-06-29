#ifndef RM_HPP
#define RM_HPP

struct RmOptions {
    int is_recursive = 0;
    int is_force = 0;
    int is_interactive = 0;
    int is_verbose = 0;
    int remove_empty_dirs = 0;
    int is_trash = 0;
};

void rm_command(int argc, char** argv);

#endif
