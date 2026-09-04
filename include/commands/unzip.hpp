#ifndef UNZIP_HPP
#define UNZIP_HPP

#include <string>
#include <vector>

struct UnzipOptions {
    std::string archive;
    std::vector<std::string> entry_names;
    bool list = false;
    bool test = false;
    bool stdout_mode = false;
    std::string target_dir;
    bool no_clobber = false;
    bool overwrite = false;
    bool quiet = false;
    bool verbose = false;
};

int unzip_command(int argc, char** argv);

#endif
