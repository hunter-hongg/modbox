#ifndef FILE_HPP
#define FILE_HPP

struct FileOptions {
    bool brief = false;
    bool no_symlinks = false;
    bool dereference = true;
    bool stdin_mode = false;
};

int file_command(int argc, char** argv);

#endif
