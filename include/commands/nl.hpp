#ifndef NL_HPP
#define NL_HPP

struct NlOptions {
    char* body_numbering = nullptr;     /* a, t, n, or pBRE */
    char* header_numbering = nullptr;
    char* footer_numbering = nullptr;
    char* number_format = nullptr;      /* ln, rn, rz */
    char* number_separator = nullptr;
    char section_delimiters[2] = {'\\', ':'};
    int line_increment = 1;
    int join_blank_lines = 0;
    int no_renumber = 0;
    int starting_line_number = 1;
    int number_width = 6;
};

void nl_command(int argc, char** argv);

#endif
