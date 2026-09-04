#ifndef ZIP_HPP
#define ZIP_HPP

#include <string>
#include <vector>

struct ZipOptions {
    std::string archive;
    std::vector<std::string> input_paths;
    bool recursive = false;
    bool junk_paths = false;
    bool update = false;
    bool freshen = false;
    bool delete_mode = false;
    bool quiet = false;
    bool verbose = false;
    std::string exclude;
    bool read_names_from_stdin = false;
    int level = 6;
};

int zip_command(int argc, char** argv);

#endif
