#ifndef GZIP_HPP
#define GZIP_HPP

#include <string>

// Resolved v1 options for the `gzip` command. Populated from argtable3
// parsing; the command fills in behavior ticket by ticket.
struct GzipOptions {
    bool decompress = false;   // -d / --decompress / --uncompress
    bool keep = false;         // -k / --keep
    bool to_stdout = false;    // -c / --stdout
    bool force = false;        // -f / --force
    bool quiet = false;        // -q / --quiet
    bool verbose = false;      // -v / --verbose
    int level = 6;             // -1..-9, --fast=1, --best=9, default 6
    bool help = false;         // -h / --help
    bool version = false;      // --version
};

int gzip_command(int argc, char** argv);

#endif
