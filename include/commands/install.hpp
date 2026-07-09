#ifndef INSTALL_HPP
#define INSTALL_HPP

#include <sys/stat.h>

struct InstallOptions {
    int is_directory = 0;
    int is_verbose = 0;
    int is_strip = 0;
    int is_compare = 0;
    int is_preserve_timestamps = 0;
    int is_backup = 0;
#define INSTALL_DEFAULT_MODE 0755
    mode_t mode = INSTALL_DEFAULT_MODE;
    int mode_set = 0;
    const char* owner = nullptr;
    const char* group = nullptr;
    const char* target_dir = nullptr;
    int no_target_directory = 0;
    const char* strip_program = nullptr;
    const char* backup_suffix = nullptr;
};

void install_command(int argc, char** argv);

#endif
