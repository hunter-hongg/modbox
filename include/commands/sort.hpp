#ifndef SORT_HPP
#define SORT_HPP

#include <string>
#include <vector>

struct ParsedKey {
    int field_start = 0;
    int char_start = 0;
    int field_end = 0;
    int char_end = 0;
    int flags = 0;
};

struct SortOptions {
    int ignore_leading_blanks = 0;   // -b
    int ignore_case = 0;             // -f
    int numeric_sort = 0;            // -n
    int reverse = 0;                 // -r
    int unique = 0;                  // -u
    int check = 0;                   // -c
    int stable = 0;                  // -s
    std::string key_spec;            // -k POS1[,POS2]
    char field_separator = 0;        // -t SEP (0 = default blank transition)
    std::string output_file;         // -o FILE
    std::vector<ParsedKey> keys;     // parsed key specs
};

void sort_command(int argc, char** argv);

#endif
