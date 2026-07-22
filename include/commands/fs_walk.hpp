#ifndef FS_WALK_HPP
#define FS_WALK_HPP

#include <string>
#include <vector>
#include <functional>
#include <sys/stat.h>

struct Dirent {
    std::string name;
    std::string full_path;
    struct stat st;
    bool valid;
};

using WalkAction = std::function<void(const Dirent&)>;
using ShouldRecurse = std::function<bool(const Dirent&)>;

void walk_directory(const char* dirpath, WalkAction action,
                    ShouldRecurse should_recurse, int max_depth);

#endif
