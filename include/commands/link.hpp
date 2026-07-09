#ifndef LINK_HPP
#define LINK_HPP

struct LinkOptions {
    int is_verbose = 0;
};

void link_command(int argc, char** argv);

#endif
