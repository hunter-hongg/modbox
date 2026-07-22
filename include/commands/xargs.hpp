#ifndef XARGS_HPP
#define XARGS_HPP

struct XargsOptions {
  int max_args = 0;
  int max_chars = 0;
  int max_procs = 1;
  int delimiter = -1;
  int replace_pos = -1;
  bool no_run_if_empty = false;
  bool null = false;
  bool show_limits = false;
};

void xargs_command(int argc, char** argv);

#endif
