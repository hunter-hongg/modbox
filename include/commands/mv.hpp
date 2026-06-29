#ifndef MV_HPP
#define MV_HPP

struct MvOptions {
    int is_interactive = 0;
    int is_no_clobber = 0;
    int is_force = 0;
    int is_verbose = 0;
    int is_update = 0;
    int is_backup = 0;
    const char* target_dir = nullptr;
    int no_target_dir = 0;
};

void mv_command(int argc, char** argv);

#endif
