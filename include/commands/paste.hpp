#ifndef PASTE_HPP
#define PASTE_HPP

struct PasteOptions {
    const char* delimiters = "\t";
    int serial = 0;
    int zero_terminated = 0;
};

void paste_command(int argc, char** argv);

#endif
