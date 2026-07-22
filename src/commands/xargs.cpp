#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands/xargs.hpp"

#define XARGS_MAX_LINE 1048576
#define XARGS_MAX_ITEMS 65536
#define XARGS_MAX_CHARS 131072

static void print_help(const char* prog) {
  printf("Usage: %s [OPTION]... COMMAND [INITIAL-ARGS]...\n", prog);
  printf("Run COMMAND with arguments INITIAL-ARGS, followed by items read from stdin.\n");
  printf("\n");
  printf("  -0, --null              items are separated by NUL, not whitespace\n");
  printf("  -I, --replace=REPLACESTR replace REPLACESTR in COMMAND with stdin items\n");
  printf("  -n, --max-args=MAX_ARGS  use at most MAX_ARGS args per command\n");
  printf("  -s, --max-chars=MAX_CHARS  use at most MAX_CHARS per command line\n");
  printf("  -P, --max-procs=MAX_PROCS  run up to MAX_PROCS processes at once\n");
  printf("      --help              display this help and exit\n");
  printf("      --version           output version information and exit\n");
}

static void split_line(const char* line, std::vector<std::string>& out, bool null_delim) {
  if (null_delim) {
    const char* p = line;
    size_t len = strlen(line);
    const char* start = p;
    while (1) {
      if (*p == '\0' || *p == '\n') {
        out.push_back(std::string(start, p - start));
        while (*p == '\0') p++;
        if (*p == '\n') p++;
        start = p;
        if (*start == '\0') break;
      } else {
        p++;
      }
    }
    return;
  }
  std::istringstream iss(line);
  std::string word;
  while (iss >> word) {
    out.push_back(word);
  }
}

static size_t args_length(const std::vector<std::string>& args) {
  size_t total = 0;
  for (const auto& a : args) {
    total += a.size() + 1;
  }
  return total;
}

static int run_cmd(const char** cmd, const XargsOptions* opts) {
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "xargs: fork failed: %s\n", strerror(errno));
    return 0;
  }
  if (pid == 0) {
    execvp(cmd[0], (char* const*)cmd);
    fprintf(stderr, "xargs: %s: %s\n", cmd[0], strerror(errno));
    _exit(127);
  }
  int status = 0;
  pid_t r = waitpid(pid, &status, 0);
  if (r < 0) {
    fprintf(stderr, "xargs: waitpid failed: %s\n", strerror(errno));
    return 0;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return 0;
}

void xargs_command(int argc, char** argv) {
  struct XargsOptions opts = {0, 0, 1, -1, -1, false, false, false};
  bool replace_str_set = false;
  std::string replace_str;
  std::vector<const char*> initial_args;
  std::vector<std::string> stdin_items;
  const char* stdin_items_file = "-";
  bool have_stdin = false;

  int i = 1;
  for (; i < argc; i++) {
    const char* a = argv[i];
    if (strcmp(a, "--help") == 0) { print_help(argv[0]); return; }
    if (strcmp(a, "--version") == 0) { printf("xargs (modbox) 1.0\n"); return; }
    if (strcmp(a, "-0") == 0 || strcmp(a, "--null") == 0) { opts.null = true; continue; }
    if (strcmp(a, "--show-limits") == 0) { opts.show_limits = true; continue; }
    if (strcmp(a, "-I") == 0 && i + 1 < argc) { replace_str = argv[++i]; replace_str_set = true; continue; }
    if (strncmp(a, "-I", 2) == 0 && a[2]) { replace_str = a + 2; replace_str_set = true; continue; }
    if (strcmp(a, "-n") == 0 && i + 1 < argc) { opts.max_args = std::atoi(argv[++i]); continue; }
    if (strcmp(a, "-s") == 0 && i + 1 < argc) { opts.max_chars = std::atoi(argv[++i]); continue; }
    if (strcmp(a, "-P") == 0 && i + 1 < argc) { opts.max_procs = std::atoi(argv[++i]); continue; }
    if (strcmp(a, "-t") == 0) {
      have_stdin = true;
      stdin_items_file = "-";
      continue;
    }
    if (a[0] == '-' && a[1] != '-' && a[1] != '\0') {
      fprintf(stderr, "xargs: unrecognized option '%s'\n", a);
      fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
      return;
    }
    break;
  }
  for (; i < argc; i++) {
    initial_args.push_back(argv[i]);
  }
  if (initial_args.empty() && !replace_str_set) {
    fprintf(stderr, "xargs: missing command\n");
    fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
    return;
  }

  FILE* fp = stdin;
  char line_buf[XARGS_MAX_LINE];
  if (fgets(line_buf, sizeof(line_buf), fp)) {
    size_t len = strlen(line_buf);
    if (len > 0 && line_buf[len - 1] == '\n') line_buf[--len] = '\0';
    std::vector<std::string> items;
    split_line(line_buf, items, opts.null);
    stdin_items.insert(stdin_items.end(), items.begin(), items.end());
  }
  while (fgets(line_buf, sizeof(line_buf), fp)) {
    size_t len = strlen(line_buf);
    if (len > 0 && line_buf[len - 1] == '\n') line_buf[--len] = '\0';
    std::vector<std::string> items;
    split_line(line_buf, items, opts.null);
    stdin_items.insert(stdin_items.end(), items.begin(), items.end());
  }

  if (opts.show_limits) {
    printf("Your getprocesslimits() are:\n");
    printf("maximum length of command line arguments = %d\n", XARGS_MAX_CHARS);
    printf("maximum number of arguments to a command = %d\n", XARGS_MAX_ITEMS);
    printf("execution time per command (seconds) = unlimited\n");
    printf("maximum combined length of environment variables = unlimited\n");
    return;
  }

  if (stdin_items.empty() && !replace_str_set) {
    if (initial_args.empty()) return;
    const char** cmd = new const char*[initial_args.size() + 1];
    for (size_t k = 0; k < initial_args.size(); k++) cmd[k] = initial_args[k];
    cmd[initial_args.size()] = nullptr;
    run_cmd(cmd, &opts);
    delete[] cmd;
    return;
  }

  if (replace_str_set && !initial_args.empty()) {
    int ret = 0;
    for (size_t idx = 0; idx < stdin_items.size(); idx++) {
      std::string cmd_str;
      for (size_t k = 0; k < initial_args.size(); k++) {
        if (k > 0) cmd_str += " ";
        const char* a = initial_args[k];
        bool replaced = false;
        if (strcmp(a, replace_str.c_str()) == 0) {
          cmd_str += stdin_items[idx];
          replaced = true;
        }
        if (!replaced) cmd_str += a;
      }
      int r = system(cmd_str.c_str());
      if (r != 0) ret = r;
    }
    return;
  }

  if (replace_str_set && initial_args.empty()) {
    for (size_t idx = 0; idx < stdin_items.size(); idx++) {
      int r = system(stdin_items[idx].c_str());
      if (r != 0) {
        if (opts.max_procs == 1) return;
      }
    }
    return;
  }

  int nargs = opts.max_args > 0 ? opts.max_args : XARGS_MAX_ITEMS;
  int max_c = opts.max_chars > 0 ? opts.max_chars : XARGS_MAX_CHARS;

  size_t pos = 0;
  int last_ret = 0;
  while (pos < stdin_items.size()) {
    std::vector<std::string> batch;
    size_t used_chars = 0;
    for (size_t k = 0; k < initial_args.size(); k++) {
      used_chars += strlen(initial_args[k]) + 1;
    }
    for (int n = 0; pos < stdin_items.size() && (opts.max_args <= 0 || n < nargs); n++, pos++) {
      if ((int)batch.size() > 0) used_chars++;
      used_chars += stdin_items[pos].size() + 1;
      if (used_chars > (size_t)max_c) {
        pos--;
        break;
      }
      batch.push_back(stdin_items[pos]);
    }
    if (opts.no_run_if_empty && batch.empty()) return;

    int total = (int)(initial_args.size() + batch.size());
    const char** cmd = new const char*[total + 1];
    for (size_t k = 0; k < initial_args.size(); k++) cmd[k] = initial_args[k];
    for (size_t k = 0; k < batch.size(); k++) cmd[initial_args.size() + k] = batch[k].c_str();
    cmd[total] = nullptr;
    last_ret = run_cmd(cmd, &opts);
    delete[] cmd;
  }
}
