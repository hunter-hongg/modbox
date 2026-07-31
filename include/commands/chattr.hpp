#ifndef CHATTR_HPP
#define CHATTR_HPP

#include <sys/types.h>
#include <sys/ioctl.h>
#include <argtable3.h>

struct ChattrOptions {
    int is_recursive = 0;
    int is_verbose = 0;
    int is_silent = 0;
    int preserve_root = 0;
    int no_preserve_root = 0;
    struct arg_lit *recursive_opt = nullptr;
    struct arg_lit *verbose_opt = nullptr;
    struct arg_lit *silent_opt = nullptr;
    struct arg_lit *preserve_root_opt = nullptr;
    struct arg_lit *no_preserve_root_opt = nullptr;
    struct arg_lit *help_opt = nullptr;
    struct arg_file *files_arg = nullptr;
    struct arg_str *project_opt = nullptr;
    struct arg_str *version_num_opt = nullptr;
    const char* project_id = nullptr;
    const char* version_str = nullptr;
    int use_project = 0;
    int use_version = 0;
};

int chattr_command(int argc, char** argv);

#endif
