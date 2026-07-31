#ifndef MKTEMP_HPP
#define MKTEMP_HPP

struct MktempOptions {
  const char* template_prefix = nullptr;
};

int mktemp_command(int argc, char** argv);

#endif
