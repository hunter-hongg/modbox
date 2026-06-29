#ifndef LS_HPP
#define LS_HPP

enum class ColorMode { NEVER, ALWAYS, AUTO };

struct LsOptions {
    int show_all = 0;
    int show_almost_all = 0;
    int show_details = 0;
    int show_author = 0;
    int escape_mode = 0;
    int ignore_backups = 0;
    int list_dir_contents = 1;
    int show_columns = 1;
    int reverse_sort = 0;
    int unsorted = 0;
    int show_one_column = 0;
    int classify = 0;
    int colorful = 0;
    int show_icons = 0;
    ColorMode color_mode = ColorMode::NEVER;
    unsigned long block_size = 0;
    char size_suffix = 0;
};

void ls_command(int argc, char** argv);

#endif
