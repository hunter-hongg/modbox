#ifndef ZSTD_HPP
#define ZSTD_HPP

constexpr int ZSTD_DEFAULT_LEVEL = 3;

struct ZstdOptions {
    bool decompress = false;   // -d / --decompress
    bool keep = false;         // -k / --keep
    bool force = false;        // -f / --force
    bool quiet = false;        // -q / --quiet
    bool verbose = false;      // -v / --verbose
    bool to_stdout = false;    // -c / --stdout
    bool rm_source = false;    // --rm
    int level = ZSTD_DEFAULT_LEVEL;  // -1 .. -22
    bool version = false;      // --version
    bool help = false;         // -h / --help
    bool list_info = false;    // -l / --list
    int dict_id = 0;           // -D <id>
    bool no_progress = false;  // --no-progress
};

int zstd_command(int argc, char** argv);

#endif // ZSTD_HPP
