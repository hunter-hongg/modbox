#ifndef GREP_TUI_HPP
#define GREP_TUI_HPP

#include <string>
#include <vector>

struct GrepMatch {
    std::string file_path;
    std::string display_name;
    int line_number;
    std::string line_content;
    size_t match_start;
    size_t match_end;
};

void grep_tui_main(int argc, char** argv);

#endif
