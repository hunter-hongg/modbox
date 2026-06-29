#ifndef FIND_HPP
#define FIND_HPP

#include <string>
#include <vector>

struct FindOptions {
    std::vector<std::string> paths;  // starting points
    std::string name_pattern;        // -name
    std::string iname_pattern;       // -iname
    char type_filter = 0;            // -type: 'f', 'd', 'l', or 0 (any)
    int empty_only = 0;              // -empty
    int max_depth = -1;             // -maxdepth, -1 = unlimited
    int min_depth = 0;              // -mindepth
    int has_action = 0;             // any action specified?
    int do_print = 0;               // -print
    int do_delete = 0;              // -delete
    int has_exec = 0;               // -exec
    std::vector<std::string> exec_args;  // exec command arguments
    int exec_plus = 0;              // -exec ... + (batch mode)
    std::vector<std::string> exec_paths; // accumulated paths for exec +
};

void find_command(int argc, char** argv);

#endif
