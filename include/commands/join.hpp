#ifndef JOIN_HPP
#define JOIN_HPP

struct JoinOptions {
  int field = 1;
  char delim = ' ';
  int file1_field = 1;
  int file2_field = 1;
};

void join_command(int argc, char** argv);

#endif
