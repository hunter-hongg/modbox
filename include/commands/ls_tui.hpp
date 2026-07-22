#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <ctime>

#include "commands/fs_classify.hpp"

struct LsOptions;

struct TuiEntry {
    std::string path;
    std::string display_name;
    FileType file_type;
    uint64_t size;
    std::string perm_str;
    std::string owner;
    std::string group;
    time_t mtime;
    std::string mtime_str;
};

enum class SortMode : uint8_t { Name, Size, Mtime, Type };

std::vector<TuiEntry> tui_collect_entries(const char* dirpath);
void ls_tui_command(int argc, char** argv);
