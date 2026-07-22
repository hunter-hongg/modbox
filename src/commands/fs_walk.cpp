#include "commands/fs_walk.hpp"
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <cstdio>

void walk_directory(const char* dirpath, WalkAction action,
                    ShouldRecurse should_recurse, int max_depth) {
    if (max_depth == 0) return;

    DIR* dir = opendir(dirpath);
    if (dir == NULL) {
        fprintf(stderr, "%s: %s\n", dirpath, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        Dirent d;
        d.name = entry->d_name;
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);
        d.full_path = full_path;
        d.valid = (lstat(full_path, &d.st) == 0);
        if (!d.valid) {
            d.st = {};
        }

        action(d);

        if (d.valid && S_ISDIR(d.st.st_mode) && should_recurse(d)) {
            walk_directory(full_path, action, should_recurse, max_depth - 1);
        }
    }

    closedir(dir);
}
