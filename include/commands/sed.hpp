#ifndef SED_HPP
#define SED_HPP

struct SedOptions {
    int suppress_print = 0;    // -n
    int extended_regex = 0;    // -E, -r
    int separate_files = 0;    // -s
    const char* in_place = nullptr;  // -i with optional suffix ("" for no suffix)
};

void sed_command(int argc, char** argv);

#endif
