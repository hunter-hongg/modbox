#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/mkdir.hpp"
#include "commands/command_macros.hpp"

#define DIR_MODE_DEFAULT 0777

static int create_dir(const char *path, mode_t mode, int is_verbose) {
  if (mkdir(path, mode) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mkdir: cannot create directory '%s': %s\n", path,
                  strerror(errno));
    return -1;
  }

  if (is_verbose) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)printf("mkdir: created directory '%s'\n", path);
  }

  return 0;
}

static int create_dir_parents(const char *path, mode_t mode, int is_verbose) {
  char *path_copy = strdup(path);
  char *p = path_copy;
  char *sep;

  /* Skip leading slash, we already have / */
  if (*p == '/') {
    p++;
  }

  while ((sep = strchr(p, '/')) != NULL) {
    /* Skip consecutive slashes (e.g. a//b) */
    if (sep == p) {
      p = sep + 1;
      continue;
    }

    *sep = '\0';

    struct stat st;
    if (stat(path_copy, &st) == 0) {
      if (!S_ISDIR(st.st_mode)) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
                      path_copy, strerror(errno));
        free(path_copy);
        return -1;
      }
    } else {
      if (mkdir(path_copy, mode) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
                      path_copy, strerror(errno));
        free(path_copy);
        return -1;
      }
      if (is_verbose) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)printf("mkdir: created directory '%s'\n", path_copy);
      }
    }

    *sep = '/';
    p = sep + 1;
  }

  /* Create the final component */
  struct stat st;
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      /* Already exists — not an error with -p */
      free(path_copy);
      return 0;
    }
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mkdir: cannot create directory '%s': File exists\n",
                  path);
    free(path_copy);
    return -1;
  }

  int ret = create_dir(path, mode, is_verbose);
  free(path_copy);
  return ret;
}

void mkdir_command(int argc, char **argv) {
  struct arg_lit *parents_opt =
      arg_lit0("p", "parents",
               "no error if existing, make parent directories as needed");
  struct arg_lit *verbose_opt =
      arg_lit0("v", "verbose", "print a message for each created directory");
  struct arg_str *mode_opt =
      arg_str0("m", "mode", "MODE",
               "set file mode (as in chmod), not a=rwx - umask");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *dirs_arg =
      arg_filen(NULL, NULL, "DIRECTORY...", 1, 1000, "directories to create");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {parents_opt, verbose_opt, mode_opt, help_opt, dirs_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... DIRECTORY...\n", argv[0]);
    printf("Create the DIRECTORY(ies), if they do not already exist.\n");
    printf("\n");
    printf("  -m, --mode=MODE    set file mode (as in chmod), not a=rwx - umask\n");
    printf("  -p, --parents      no error if existing, make parent directories as needed\n");
    printf("  -v, --verbose      print a message for each created directory\n");
    printf("  -h, --help         display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  MkdirOptions opts = {};
  opts.is_parents = (parents_opt->count > 0);
  opts.is_verbose = (verbose_opt->count > 0);
  opts.mode = DIR_MODE_DEFAULT;

  if (mode_opt->count > 0) {
    /* Parse octal mode string */
    char *endptr = NULL;
    long m = strtol(mode_opt->sval[0], &endptr, 8);
    if (*endptr != '\0' || m < 0 || m > 07777) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mkdir: invalid mode '%s'\n", mode_opt->sval[0]);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    opts.mode = (mode_t)(m & 07777);
  }

  int num_dirs = dirs_arg->count;

  for (int i = 0; i < num_dirs; i++) {
    const char *dirpath = dirs_arg->filename[i];

    if (opts.is_parents) {
      if (create_dir_parents(dirpath, opts.mode, opts.is_verbose) != 0) {
        continue;
      }
    } else {
      struct stat st;
      if (stat(dirpath, &st) == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr,
                      "mkdir: cannot create directory '%s': File exists\n",
                      dirpath);
        continue;
      }
      create_dir(dirpath, opts.mode, opts.is_verbose);
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("mkdir", mkdir_command, "Create directories");
