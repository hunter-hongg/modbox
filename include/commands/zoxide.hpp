#ifndef ZOXIDE_HPP
#define ZOXIDE_HPP

#include <cstdint>
#include <string>
#include <vector>

struct ZoxideEntry {
    std::string path;
    double rank = 0.0;
    int64_t timestamp = 0;
};

void zoxide_command(int argc, char** argv);

#endif
