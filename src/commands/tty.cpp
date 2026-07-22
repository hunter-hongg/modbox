#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands/tty.hpp"
#include "commands/command_macros.hpp"

void tty_command(int argc, char** argv) {
  struct arg_lit* silent_opt =
      arg_lit0("s", "silent,quiet", "print nothing, only return an exit status");
  struct arg_lit* help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_lit* version_opt =
      arg_lit0(NULL, "version", "output version information and exit");
  struct arg_end* end = arg_end(20);

  void* argtable[] = {silent_opt, help_opt, version_opt, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]...\n", argv[0]);
    printf("Print the file name of the terminal connected to standard input.\n");
    printf("\n");
    printf("  -s, --silent, --quiet   print nothing, only return an exit status\n");
    printf("      --help              display this help and exit\n");
    printf("      --version           output version information and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (version_opt->count > 0) {
    printf("tty (modbox) 1.0\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  int is_terminal = isatty(STDIN_FILENO);
  int exit_status;

  if (is_terminal) {
    char* name = ttyname(STDIN_FILENO);
    if (name == NULL) {
      if (silent_opt->count == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "tty: standard input: %s\n", strerror(errno));
      }
      exit_status = 2;
    } else {
      if (silent_opt->count == 0) {
        printf("%s\n", name);
      }
      exit_status = 0;
    }
  } else {
    if (silent_opt->count == 0) {
      printf("not a tty\n");
    }
    exit_status = 1;
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

  if (exit_status != 0) {
    exit(exit_status);
  }
}

REGISTER_COMMAND("tty", tty_command, "Print terminal file name");
