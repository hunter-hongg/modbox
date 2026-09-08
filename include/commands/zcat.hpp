#ifndef ZCAT_HPP
#define ZCAT_HPP

// zcat command options
struct ZcatOptions {
    bool help = false;       // -h / --help
    bool version = false;    // --version
};

int zcat_command(int argc, char** argv);

#endif // ZCAT_HPP