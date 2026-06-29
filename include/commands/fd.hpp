#ifndef FD_HPP
#define FD_HPP

#include <string>
#include <vector>

enum class FdColorMode { NEVER, ALWAYS, AUTO };

struct FdOptions {
    int hidden = 0;
    int no_ignore = 0;
    int case_sensitive = 0;
    int ignore_case = 0;
    int smart_case = 0;
    int glob_mode = 0;
    int full_path = 0;
    int follow = 0;
    int print0 = 0;
    int max_depth = -1;
    int max_results = 0;
    FdColorMode color_mode = FdColorMode::AUTO;
    char type_filter = 0;
    std::string pattern;
    std::vector<std::string> extensions;
    std::vector<std::string> exclude;
    std::vector<std::string> exclude_patterns; // pre-compiled glob patterns
    int has_exec = 0;
    int exec_batch = 0;
    std::vector<std::string> exec_args;
    std::vector<std::string> exec_paths;
};

void fd_command(int argc, char** argv);

#endif
