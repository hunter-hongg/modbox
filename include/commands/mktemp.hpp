#ifndef MKTEMP_HPP
#define MKTEMP_HPP

struct MktempOptions {
  const char* template_prefix = nullptr;
};

void mktemp_command(int argc, char** argv);

#endif
