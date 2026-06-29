#ifndef TAC_HPP
#define TAC_HPP

struct TacOptions {
    int before_mode = 0;
    int regex_mode = 0;
    const char* separator = nullptr;
};

void tac_command(int argc, char** argv);

#endif
