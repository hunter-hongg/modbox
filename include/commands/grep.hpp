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
};

void grep_command(int argc, char** argv);
void grep_tui_main(int argc, char** argv);

#endif
