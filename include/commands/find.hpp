#ifndef FIND_HPP
#define FIND_HPP

#include <string>
#include <vector>
#include <ctime>
#include <cstdint>

struct FindMatch;

struct FindOptions {
    std::vector<std::string> paths;
    std::string name_pattern;
    std::string iname_pattern;
    char type_filter = 0;
    int empty_only = 0;
    int max_depth = -1;
    int min_depth = 0;
    int has_action = 0;
    int do_print = 0;
    int do_delete = 0;
    int has_exec = 0;
    std::vector<std::string> exec_args;
    int exec_plus = 0;
    std::vector<std::string> exec_paths;
    int tui_mode = 0;
    std::vector<FindMatch>* collect_results = nullptr;
};

struct FindMatch {
    std::string path;
    std::string display_name;
    int file_type;
    uint64_t size;
    time_t mtime;
    std::string mtime_str;
    std::string perm_str;
};

int find_parse_args(int argc, char** argv, FindOptions* opts,
                    int* help_requested);
void find_usage(const char* progname);

std::vector<FindMatch> find_collect_matches(FindOptions* opts);

void find_command(int argc, char** argv);
void find_tui_main(int argc, char** argv);

#endif
