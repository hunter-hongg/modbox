#ifndef LS_ENTRY_HPP
#define LS_ENTRY_HPP

#include <string>
#include <vector>
#include <sys/stat.h>

struct LsEntry {
    std::string path;
    std::string display_name;
    struct stat st;

    mode_t mode() const { return st.st_mode; }
    bool is_dir() const { return S_ISDIR(st.st_mode); }
    bool is_symlink() const { return S_ISLNK(st.st_mode); }
    bool is_executable() const { return st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH); }
    off_t size() const { return st.st_size; }
    time_t mtime() const { return st.st_mtime; }
    uid_t uid() const { return st.st_uid; }
    gid_t gid() const { return st.st_gid; }
    nlink_t nlink() const { return st.st_nlink; }
};

std::vector<LsEntry> ls_collect_entries(const char* dirpath, int show_all,
                                     int show_almost_all, int ignore_backups);

#endif
