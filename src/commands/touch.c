#include <argtable3.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "commands/touch.h"

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void touch_command(gint argc, gchar **argv) {
  struct arg_lit *only_atime_opt =
      arg_lit0("a", NULL, "change only the access time");
  struct arg_lit *only_mtime_opt =
      arg_lit0("m", NULL, "change only the modification time");
  struct arg_lit *no_create_opt =
      arg_lit0("c", "no-create", "do not create any files");
  struct arg_str *reference_opt =
      arg_str0("r", "reference", "FILE",
               "use this file's times instead of current time");
  struct arg_str *date_opt =
      arg_str0("d", "date", "STRING",
               "parse STRING and use it instead of current time");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "FILE...", 1, 1000, "files to touch");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {only_atime_opt, only_mtime_opt, no_create_opt,
                      reference_opt, date_opt, help_opt, files_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... FILE...\n", argv[0]);
    printf("Update the access and modification times of each FILE to the current time.\n");
    printf("\n");
    printf("  -a                     change only the access time\n");
    printf("  -c, --no-create        do not create any files\n");
    printf("  -d, --date=STRING      parse STRING and use it instead of current time\n");
    printf("  -m                     change only the modification time\n");
    printf("  -r, --reference=FILE   use this file's times instead of current time\n");
    printf("  -h, --help             display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  TouchOptions opts = {0};
  opts.only_atime = (only_atime_opt->count > 0);
  opts.only_mtime = (only_mtime_opt->count > 0);
  opts.no_create = (no_create_opt->count > 0);
  opts.reference = (reference_opt->count > 0) ? reference_opt->sval[0] : NULL;
  opts.timestamp = (date_opt->count > 0) ? date_opt->sval[0] : NULL;

  int num_files = files_arg->count;
  if (num_files < 1) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "touch: missing file operand\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  /* If neither -a nor -m specified, change both */
  int change_atime = 1;
  int change_mtime = 1;
  if (opts.only_atime && !opts.only_mtime) {
    change_mtime = 0;
  }
  if (opts.only_mtime && !opts.only_atime) {
    change_atime = 0;
  }

  struct timespec ref_times[2];
  int use_ref = 0;

  if (opts.reference != NULL) {
    struct stat ref_stat;
    if (stat(opts.reference, &ref_stat) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "touch: '%s': No such file or directory\n",
                    opts.reference);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    ref_times[0] = ref_stat.st_atim;
    ref_times[1] = ref_stat.st_mtim;
    use_ref = 1;
  }

  for (int i = 0; i < num_files; i++) {
    const char *path = files_arg->filename[i];

    int fd = -1;
    struct stat st;
    int exists = (stat(path, &st) == 0);

    if (!exists) {
      if (opts.no_create) {
        continue;
      }
      /* Create empty file */
      fd = open(path, O_WRONLY | O_CREAT | O_EXCL,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (fd < 0 && errno == EEXIST) {
        /* Race: file appeared between stat and open */
        fd = open(path, O_WRONLY, 0);
      }
      if (fd < 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "touch: cannot touch '%s': %s\n", path,
                      strerror(errno));
        continue;
      }
      close(fd);
      /* Re-stat now that file exists */
      if (stat(path, &st) != 0) {
        continue;
      }
    }

    /* Build times array */
    struct timespec times[2];

    if (use_ref) {
      times[0] = ref_times[0];
      times[1] = ref_times[1];
    } else {
      /* Current time */
      struct timespec now;
      // NOLINTNEXTLINE(misc-include-cleaner)
      clock_gettime(CLOCK_REALTIME, &now);
      times[0] = now;
      times[1] = now;
    }

    /* Preserve the time we're NOT changing */
    if (!change_atime) {
      times[0] = st.st_atim;
    }
    if (!change_mtime) {
      times[1] = st.st_mtim;
    }

    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)utimensat(AT_FDCWD, path, times, 0);
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
