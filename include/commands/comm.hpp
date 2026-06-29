#ifndef COMM_HPP
#define COMM_HPP

struct CommOptions {
    int suppress_col1 = 0;      // -1
    int suppress_col2 = 0;      // -2
    int suppress_col3 = 0;      // -3
    int ignore_case = 0;        // -i
    int check_order = 1;        // --check-order (default: on)
    int nocheck_order = 0;      // --nocheck-order
    const char* output_delimiter = "\t"; // --output-delimiter
};

void comm_command(int argc, char** argv);

#endif
