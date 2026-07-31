#ifndef UNLINK_HPP
#define UNLINK_HPP

struct UnlinkOptions {
    int is_verbose = 0;
};

int unlink_command(int argc, char** argv);

#endif
