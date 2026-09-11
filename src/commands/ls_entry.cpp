#include "commands/ls_entry.hpp"
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <sys/stat.h>
#include <utility>

std::vector<LsEntry> ls_collect_entries(const char* dirpath, int show_all,
                                     int show_almost_all, int ignore_backups) {
    std::vector<LsEntry> result;
    DIR* dir = opendir(dirpath);
    if (dir == NULL) {
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if ((show_all == 0) && entry->d_name[0] == '.') {
            if (show_almost_all == 0) {
                continue;
            }
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }
        }
        if (ignore_backups != 0) {
            size_t const dlen = strlen(entry->d_name);
            if (dlen > 0 && entry->d_name[dlen - 1] == '~') {
                continue;
            }
        }

        LsEntry e;
        char full_path[4096];
        (void)snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);
        e.path = full_path;
        e.display_name = entry->d_name;
        if (lstat(full_path, &e.st) == 0) {
            result.push_back(std::move(e));
        } else {
            e.st = {};
            result.push_back(std::move(e));
        }
    }

    closedir(dir);
    return result;
}
