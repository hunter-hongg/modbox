#ifndef RMDIR_HPP
#define RMDIR_HPP

#include <sys/stat.h>

struct RmdirOptions {
  int is_parents = 0;
};

void rmdir_command(int argc, char** argv);

#endif
