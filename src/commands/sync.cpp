#include <argtable3.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands/sync.hpp"

void sync_command(int argc, char **argv) {
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_lit *version_opt =
      arg_lit0("v", "version", "output version information and exit");
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "FILE", 0, 1000, "files to sync");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {help_opt, version_opt, files_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION] [FILE]...\n", argv[0]);
    printf("Synchronize cached writes to persistent storage.\n");
    printf("\n");
    printf("If one or more files are specified, sync only those files.\n");
    printf("With no files, sync all filesystems.\n");
    printf("\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -v, --version  output version information and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (version_opt->count > 0) {
    printf("sync (modbox) 1.0\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  int num_files = files_arg->count;

  if (num_files == 0) {
    // sync all filesystems
    sync();
  } else {
    // sync specific files
    for (int i = 0; i < num_files; i++) {
      const char *path = files_arg->filename[i];
      int fd = open(path, O_RDONLY);
      if (fd < 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "sync: cannot open '%s': %s\n", path,
                      strerror(errno));
        continue;
      }
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)fsync(fd);
      close(fd);
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
