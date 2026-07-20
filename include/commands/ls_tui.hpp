#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct LsOptions;

struct TuiEntry {
  std::string path;
  std::string display_name;
  int is_dir;
  int is_symlink;
  int is_executable;
  uint64_t size;
  std::string perm_str;
  std::string owner;
  std::string group;
  std::string mtime_str;
};

std::vector<TuiEntry> tui_collect_entries(const char* dirpath);
void ls_tui_command(int argc, char** argv, const LsOptions* opts);
