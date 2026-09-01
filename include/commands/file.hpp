#ifndef FILE_HPP
#define FILE_HPP

struct FileOptions {
    bool brief = false;
    bool dereference = true;
};

int file_command(int argc, char** argv);

#endif
