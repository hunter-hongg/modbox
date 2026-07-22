#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "commands/mknod.hpp"
#include "commands/command_macros.hpp"

#define MKNOD_MODE_DEFAULT 0666

void mknod_command(int argc, char **argv) {
  struct arg_str *mode_opt =
      arg_str0("m", "mode", "MODE",
               "set file mode (as in chmod), not a=rw - umask");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_str *name_arg =
      arg_str1(NULL, NULL, "NAME", "special file name to create");
  struct arg_str *type_arg =
      arg_str1(NULL, NULL, "TYPE", "type (b, c, u, or p)");
  struct arg_str *major_arg =
      arg_strn(NULL, NULL, "MAJOR", 0, 1, "major device number");
  struct arg_str *minor_arg =
      arg_strn(NULL, NULL, "MINOR", 0, 1, "minor device number");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {mode_opt, help_opt, name_arg, type_arg,
                      major_arg, minor_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... NAME TYPE [MAJOR MINOR]\n", argv[0]);
    printf("Create the special file NAME of the given TYPE.\n");
    printf("\n");
    printf(
        "  -m, --mode=MODE    set file mode (as in chmod), not a=rw - umask\n");
    printf("  -h, --help         display this help and exit\n");
    printf("\n");
    printf("TYPE may be:\n");
    printf("  b      create a block (buffered) special file\n");
    printf("  c, u   create a character (unbuffered) special file\n");
    printf("  p      create a FIFO (named pipe)\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  MknodOptions opts = {};
  opts.mode = MKNOD_MODE_DEFAULT;

  if (mode_opt->count > 0) {
    char *endptr = NULL;
    long m = strtol(mode_opt->sval[0], &endptr, 8);
    if (*endptr != '\0' || m < 0 || m > 07777) {
      (void)fprintf(stderr, "mknod: invalid mode '%s'\n", mode_opt->sval[0]);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    opts.mode = (mode_t)(m & 07777);
  }

  const char *name = name_arg->sval[0];
  const char *type = type_arg->sval[0];

  if (strcmp(type, "p") == 0) {
    /* FIFO — no device numbers */
    if (major_arg->count > 0 || minor_arg->count > 0) {
      const char *extra = major_arg->count > 0 ? major_arg->sval[0]
                                               : minor_arg->sval[0];
      (void)fprintf(stderr, "mknod: extra operand '%s'\n", extra);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    if (mkfifo(name, opts.mode) != 0) {
      (void)fprintf(stderr, "mknod: cannot create '%s': %s\n", name,
                    strerror(errno));
    }
  } else if (strcmp(type, "b") == 0 || strcmp(type, "c") == 0 ||
             strcmp(type, "u") == 0) {
    /* Block or character device — need MAJOR and MINOR */
    if (major_arg->count != 1 || minor_arg->count != 1) {
      (void)fprintf(stderr, "mknod: missing device number for '%s'\n", name);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    char *endptr = NULL;
    long major_num = strtol(major_arg->sval[0], &endptr, 0);
    if (*endptr != '\0' || major_num < 0) {
      (void)fprintf(stderr, "mknod: invalid major device number '%s'\n",
                    major_arg->sval[0]);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    long minor_num = strtol(minor_arg->sval[0], &endptr, 0);
    if (*endptr != '\0' || minor_num < 0) {
      (void)fprintf(stderr, "mknod: invalid minor device number '%s'\n",
                    minor_arg->sval[0]);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    mode_t file_type = (type[0] == 'b') ? S_IFBLK : S_IFCHR;
    dev_t dev = makedev((unsigned int)major_num, (unsigned int)minor_num);

    if (mknod(name, file_type | opts.mode, dev) != 0) {
      (void)fprintf(stderr, "mknod: cannot create '%s': %s\n", name,
                    strerror(errno));
    }
  } else {
    (void)fprintf(stderr, "mknod: invalid device type '%s'\n", type);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("mknod", mknod_command, "Create device node");
