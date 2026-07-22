#include <argtable3.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <sys/stat.h>
#include <vector>
#include <string>

#include "commands/rmdir.hpp"
#include "commands/command_macros.hpp"

static int remove_dir(const char* path) {
  struct stat st;
  if (stat(path, &st) == -1) {
    fprintf(stderr, "rmdir: %s: %s\n", path, strerror(errno));
    return -1;
  }
  if (!S_ISDIR(st.st_mode)) {
    fprintf(stderr, "rmdir: %s: Not a directory\n", path);
    return -1;
  }
  if (rmdir(path) == -1) {
    fprintf(stderr, "rmdir: %s: %s\n", path, strerror(errno));
    return -1;
  }
  return 0;
}

static int rmdir_parents(const char* path) {
  std::vector<std::string> stack;
  std::string current(path);

  while (true) {
    struct stat st;
    if (stat(current.c_str(), &st) != 0) {
      break;
    }
    if (!S_ISDIR(st.st_mode)) {
      fprintf(stderr, "rmdir: %s: Not a directory\n", current.c_str());
      return -1;
    }
    stack.push_back(current);

    if (current == "/") {
      break;
    }
    size_t len = current.size();
    while (len > 0 && current[len - 1] == '/') {
      --len;
    }
    if (len == 0) {
      break;
    }
    size_t last_slash = current.rfind('/', len - 1);
    if (last_slash == std::string::npos || last_slash == 0) {
      break;
    }
    current = current.substr(0, last_slash);
  }

  bool seen_path = false;
  for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
    if (seen_path && *it == *stack.rbegin()) {
      break;
    }
    seen_path = true;
    if (rmdir(it->c_str()) != 0) {
      if (errno == ENOTEMPTY || errno == EEXIST) {
        return 0;
      }
      fprintf(stderr, "rmdir: %s: %s\n", it->c_str(), strerror(errno));
      return -1;
    }
  }
  return 0;
}

void rmdir_command(int argc, char** argv) {
  struct arg_lit* parents_opt = arg_lit0("p", "parents",
    "remove DIRECTORY and its ancestors, e.g., `rmdir -p a/b/c` is "
    "like `rmdir a/b/c a/b a`");
  struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
  struct arg_file* dirs_arg = arg_filen(NULL, NULL, "DIRECTORY...", 1, 100,
    "directories to remove");
  struct arg_end* end = arg_end(20);

  void* argtable[] = {parents_opt, help_opt, dirs_arg, end};
  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... DIRECTORY...\n", argv[0]);
    printf("Remove the DIRECTORY(ies), if they are empty.\n");
    printf("\n");
    printf(" -p, --parents remove DIRECTORY and its ancestors, e.g., "
           "`rmdir -p a/b/c` is like `rmdir a/b/c a/b a`\n");
    printf(" -h, --help display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  RmdirOptions opts;
  opts.is_parents = (parents_opt->count > 0);

  for (int i = 0; i < dirs_arg->count; i++) {
    const char* dirpath = dirs_arg->filename[i];
    if (opts.is_parents) {
      rmdir_parents(dirpath);
    } else {
      remove_dir(dirpath);
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("rmdir", rmdir_command, "Remove empty directories");
