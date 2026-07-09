#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/mkfifo.hpp"

#define FIFO_MODE_DEFAULT 0666

void mkfifo_command(int argc, char **argv) {
  struct arg_str *mode_opt =
      arg_str0("m", "mode", "MODE",
               "set file mode (as in chmod), not a=rw - umask");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *names_arg =
      arg_filen(NULL, NULL, "NAME...", 1, 1000, "FIFO names to create");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {mode_opt, help_opt, names_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... NAME...\n", argv[0]);
    printf("Create named pipes (FIFOs) with the given NAMEs.\n");
    printf("\n");
    printf("  -m, --mode=MODE    set file mode (as in chmod), not a=rw - umask\n");
    printf("  -h, --help         display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  MkfifoOptions opts = {};
  opts.mode = FIFO_MODE_DEFAULT;

  if (mode_opt->count > 0) {
    char *endptr = NULL;
    long m = strtol(mode_opt->sval[0], &endptr, 8);
    if (*endptr != '\0' || m < 0 || m > 07777) {
      (void)fprintf(stderr, "mkfifo: invalid mode '%s'\n", mode_opt->sval[0]);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    opts.mode = (mode_t)(m & 07777);
  }

  int num_names = names_arg->count;

  for (int i = 0; i < num_names; i++) {
    const char *path = names_arg->filename[i];

    if (mkfifo(path, opts.mode) != 0) {
      (void)fprintf(stderr, "mkfifo: cannot create fifo '%s': %s\n", path,
                    strerror(errno));
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
