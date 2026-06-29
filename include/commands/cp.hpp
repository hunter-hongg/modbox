#ifndef CP_HPP
#define CP_HPP

#include <sys/stat.h>

struct CpOptions {
    int is_recursive = 0;
    int is_verbose = 0;
    int is_force = 0;
    int is_no_clobber = 0;
    int is_interactive = 0;
    int is_update = 0;
    int is_preserve = 0;
    const char* target_dir = nullptr;
    const struct stat* src_stat = nullptr;
};

void cp_command(int argc, char** argv);

#endif
