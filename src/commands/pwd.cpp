#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <string>

#include "commands/pwd.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static int dirs_same(const char* a, const char* b) {
  struct stat sa;
  struct stat sb;
  if (stat(a, &sa) != 0) return 0;
  if (stat(b, &sb) != 0) return 0;
  return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

int pwd_command(int argc, char** argv) {
  struct arg_lit* logical_opt =
      arg_lit0("L", "logical", "use PWD from environment, even if it contains symlinks");
  struct arg_lit* physical_opt =
      arg_lit0("P", "physical", "avoid all symlinks");
  struct arg_lit* help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_lit* version_opt =
      arg_lit0(NULL, "version", "output version information and exit");
  struct arg_end* end = arg_end(20);

  ArgTable at({logical_opt, physical_opt, help_opt, version_opt, end});

  int nerrors = at.parse(argc, argv);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]...\n", argv[0]);
    printf("Print the full filename of the current working directory.\n");
    printf("\n");
    printf("  -L, --logical   use PWD from environment, even if it contains symlinks\n");
    printf("  -P, --physical  avoid all symlinks\n");
    printf("      --help      display this help and exit\n");
    printf("      --version   output version information and exit\n");
    return 0;
  }

  if (version_opt->count > 0) {
    print_version("pwd");
    return 0;
  }

  if (nerrors > 0) {
    return at.print_errors(end, argv[0]);
  }

  int use_physical = (physical_opt->count > 0);

  if (!use_physical) {
    const char* pwd_env = getenv("PWD");
    if (pwd_env != NULL && pwd_env[0] == '/') {
      if (dirs_same(pwd_env, ".")) {
        printf("%s\n", pwd_env);
        return 0;
      }
    }
  }

  std::error_code ec;
  std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (ec) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "pwd: %s\n", ec.message().c_str());
    return 0;
  }

  printf("%s\n", cwd.c_str());

  return 0;
}

REGISTER_COMMAND("pwd", pwd_command, "Print current working directory");
