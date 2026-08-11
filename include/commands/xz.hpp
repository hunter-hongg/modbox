#ifndef XZ_HPP
#define XZ_HPP

#include <cstdint>
#include <string>
#include <vector>

constexpr int XZ_DEFAULT_LEVEL = 6;

struct XzOptions {
    bool decompress = false;   // -d / --decompress / --uncompress
    bool keep = false;         // -k / --keep
    bool force = false;        // -f / --force
    bool quiet = false;        // -q / --quiet
    bool verbose = false;      // -v / --verbose
    bool to_stdout = false;    // -c / --stdout
    int level = XZ_DEFAULT_LEVEL;  // -0 .. -9
    bool version = false;      // --version
    bool help = false;         // -h / --help
};

int xz_command(int argc, char** argv);

#endif // XZ_HPP
