#ifndef FMT_HPP
#define FMT_HPP

struct FmtOptions {
  int width = 72;
  int uniform_spacing = 0;
};

void fmt_command(int argc, char** argv);

#endif
