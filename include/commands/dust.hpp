#ifndef DUST_HPP
#define DUST_HPP

struct DustOptions {
    int max_depth = -1;          // -d / --depth (-1 = no limit)
    int max_lines = 40;          // -n / --number-of-lines (0 = no limit)
    int show_all = 0;            // -a / --all
    int one_file_system = 0;     // -x / --one-file-system
    int si = 0;                  // -H / --si (1000 not 1024)
    int bytes = 0;               // -b / --bytes
    int no_color = 0;            // -c / --no-color
    char** exclude = nullptr;    // --exclude / -X patterns
    int exclude_count = 0;
};

void dust_command(int argc, char** argv);

#endif
