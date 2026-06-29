#ifndef CAT_HPP
#define CAT_HPP

struct CatOptions {
    int show_line_numbers = 0;
    int show_nonempty_line_numbers = 0;
    int show_ends = 0;
    int squeeze_blank = 0;
    int show_tabs = 0;
    int show_nonprinting = 0;
    int less_mode = 0;

    int blame_mode = 0;
    int highlight_mode = 0;
    int header_mode = 0;
    const char* diff_file = nullptr;

    int range_start = 0;
    int range_end = 0;
    const char* grep_pattern = nullptr;
    int context_lines = 0;
    int head_lines = 0;
    int tail_lines = 0;
    int show_stats = 0;
    int number_format = 0; // 0=decimal, 1=hex, 2=octal
};

void cat_command(int argc, char** argv);

#endif
