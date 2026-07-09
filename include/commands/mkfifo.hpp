#ifndef MKFIFO_HPP
#define MKFIFO_HPP

#include <sys/stat.h>

struct MkfifoOptions {
    mode_t mode = 0666;
};

void mkfifo_command(int argc, char** argv);

#endif
