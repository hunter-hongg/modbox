#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>

#include "commands/mktemp.hpp"

static std::string get_temp_dir() {
  const char* tmpdir = getenv("TMPDIR");
  if (tmpdir && tmpdir[0] != '\0') {
    return std::string(tmpdir);
  }
  return "/tmp";
}

static std::string rand_string(size_t len) {
  static const char charset[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static std::mt19937 rng((unsigned)std::chrono::steady_clock::now()
    .time_since_epoch().count());
  static std::uniform_int_distribution<> dist(0, (int)(sizeof(charset) - 2));
  std::string s;
  s.reserve(len);
  for (size_t i = 0; i < len; i++) {
    s += charset[(size_t)dist(rng)];
  }
  return s;
}

static int create_temp_file(std::string* out_path) {
  std::string dir = get_temp_dir();
  std::string path = dir + "/XXXXXXXXXX";

  int fd = mkstemp(path.data());
  if (fd == -1) {
    fprintf(stderr, "mktemp: failed to create temporary file: %s\n",
      strerror(errno));
    return -1;
  }
  close(fd);
  *out_path = path;
  return 0;
}

static int create_temp_file_with_prefix(std::string* out_path,
  const char* prefix) {
  std::string dir = get_temp_dir();
  std::string base = std::string(prefix) + rand_string(6);
  std::string path = dir + "/" + base;

  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd == -1) {
    fprintf(stderr, "mktemp: failed to create temporary file '%s': %s\n",
      path.c_str(), strerror(errno));
    return -1;
  }
  close(fd);
  *out_path = path;
  return 0;
}

void mktemp_command(int argc, char** argv) {
  struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
  struct arg_str* prefix_opt = arg_str0("t", "tempdir",
    "PREFIX", "interpret name as a file prefix (default: use XXXXXX template)");
  struct arg_end* end = arg_end(20);

  void* argtable[] = {help_opt, prefix_opt, end};
  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... [TEMPLATE]\n", argv[0]);
    printf("Create a unique temporary file.\n");
    printf("\n");
    printf(" -t, --tempdir=PREFIX interpret name as a file prefix\n");
    printf(" -h, --help display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  std::string out_path;
  if (prefix_opt->count > 0) {
    if (create_temp_file_with_prefix(&out_path, prefix_opt->sval[0]) != 0) {
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
  } else {
    if (create_temp_file(&out_path) != 0) {
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
  }

  printf("%s\n", out_path.c_str());
  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
