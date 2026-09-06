#ifndef GREP_HPP
#define GREP_HPP

#include <string>

enum class GrepMode { BASIC, EXTENDED, FIXED };
enum class GrepColor { NEVER, ALWAYS, AUTO };

struct GrepOptions {
    int ignore_case = 0;
    int invert_match = 0;
    int line_number = 0;
    int recursive = 0;
    int count_only = 0;
    int word_regexp = 0;
    int files_with_matches = 0;
    int line_regexp = 0;
    int only_matching = 0;
    int always_show_filename = 0;
    int never_show_filename = 0;
    GrepMode mode = GrepMode::BASIC;
    GrepColor color_mode = GrepColor::NEVER;
    std::string pattern;

    int before_context = 0;
    int after_context = 0;
    int context = 0;

    int max_count = 0;
    int quiet = 0;
    int no_messages = 0;
    int byte_offset = 0;
    int null_output = 0;
    int null_data = 0;
    std::string label;
    std::vector<std::string> include_globs;
    std::vector<std::string> exclude_globs;
    int directories_action = 0;
    std::string group_separator = "--";
    int no_group_separator = 0;
};

int grep_command(int argc, char** argv);
void grep_tui_main(int argc, char** argv);

#endif
