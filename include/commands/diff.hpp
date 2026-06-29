#ifndef DIFF_HPP
#define DIFF_HPP

struct DiffOptions {
    int brief = 0;             // -q
    int report_identical = 0;  // -s
    int ignore_case = 0;       // -i
    int ignore_all_space = 0;  // -w
    int ignore_space_change = 0; // -b
    int show_c_function = 0;   // -p
    int expand_tabs = 0;       // -t
    int color = 0;             // --color
    int context_lines = 3;     // -U or -C default
    int format = 0;            // 0=normal, 1=unified, 2=context
    int width = 130;           // --width (for side-by-side)
};

void diff_command(int argc, char** argv);

#endif
