#ifndef LN_HPP
#define LN_HPP

struct LnOptions {
    int is_verbose = 0;
    int is_force = 0;
    int is_sym = 0;
    int is_interactive = 0;
    int is_no_deref = 0;
    int is_logical = 0;
};

void ln_command(int argc, char** argv);

#endif
