#ifndef UNIQ_HPP
#define UNIQ_HPP

struct UniqOptions {
    int count = 0;
    int repeated = 0;
    int all_repeated = 0;
    int unique = 0;
    int ignore_case = 0;
    int skip_fields = 0;
    int skip_chars = 0;
    int check_chars = 0;
};

void uniq_command(int argc, char** argv);

#endif
