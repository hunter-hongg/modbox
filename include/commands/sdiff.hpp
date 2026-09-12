#ifndef SDIFF_HPP
#define SDIFF_HPP

struct SdiffOptions {
    int ignore_case = 0;             // -i
    int ignore_tab_expansion = 0;    // -E
    int ignore_trailing_space = 0;   // -Z
    int ignore_space_change = 0;     // -b
    int ignore_all_space = 0;        // -W
    int ignore_blank_lines = 0;      // -B
    const char* ignore_matching_lines = nullptr; // -I RE
    int strip_trailing_cr = 0;       // --strip-trailing-cr
    int text = 0;                    // -a
    int left_column = 0;             // -l
    int suppress_common_lines = 0;   // -s
    int expand_tabs = 0;             // -t
    int tabsize = 8;                 // --tabsize
    int width = 130;                 // -w
};

int sdiff_command(int argc, char** argv);

#endif
