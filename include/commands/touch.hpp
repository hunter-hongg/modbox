#ifndef TOUCH_HPP
#define TOUCH_HPP

struct TouchOptions {
    int only_atime = 0;
    int only_mtime = 0;
    int no_create = 0;
    const char* reference = nullptr;
    const char* timestamp = nullptr;
};

void touch_command(int argc, char** argv);

#endif
