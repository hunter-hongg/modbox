#include <argtable3.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands/sync.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/arg_util.hpp"

int sync_command(int argc, char **argv) {
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_lit *version_opt =
      arg_lit0("v", "version", "output version information and exit");
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "FILE", 0, 1000, "files to sync");
  struct arg_end *end = arg_end(20);

  ArgTable at({help_opt, version_opt, files_arg, end});

  int nerrors = at.parse(argc, argv);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION] [FILE]...\n", argv[0]);
    printf("Synchronize cached writes to persistent storage.\n");
    printf("\n");
    printf("If one or more files are specified, sync only those files.\n");
    printf("With no files, sync all filesystems.\n");
    printf("\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -v, --version  output version information and exit\n");
    return 0;
  }

  if (version_opt->count > 0) {
    print_version("sync");
    return 0;
  }

  if (nerrors > 0) {
    return at.print_errors(end, argv[0]);
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

  return 0;
}

REGISTER_COMMAND("sync", sync_command, "Flush filesystem buffers");
