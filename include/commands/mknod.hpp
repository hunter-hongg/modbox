#ifndef MKNOD_HPP
#define MKNOD_HPP

#include <sys/stat.h>

struct MknodOptions {
    mode_t mode = 0666;
};

void mknod_command(int argc, char** argv);

#endif
