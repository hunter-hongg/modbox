#include <argtable3.h>
#include <dirent.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/cp.h"

/* Buffer size for file copy operations */
#define COPY_BUF_SIZE 8192

/* Default permissions for created directories */
#define DIR_MODE 0755

/* Maximum number of file arguments (sources + dest) */
#define MAX_FILES 101

/**
 * copy_file - Copy contents of src file to dst file
 * @src: path to source file
 * @dst: path to destination file
 * @is_verbose: print progress messages if non-zero
 * @is_force: remove existing destination file if non-zero
 * @is_no_clobber: do not overwrite existing files if non-zero
 *
 * Returns: 0 on success, -1 on error
 */
static gboolean prompt_overwrite(const char *dst) {
  FILE *tty = fopen("/dev/tty", "r+");
  if (tty == NULL) {
    tty = stdin;
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stdout, "cp: overwrite '%s'? ", dst);
  } else {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(tty, "cp: overwrite '%s'? ", dst);
  }
  int c = fgetc(tty);
  if (c != '\n' && c != EOF) {
    int ch;
    do { ch = fgetc(tty); } while (ch != '\n' && ch != EOF);
  }
  if (tty != stdin) {
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(tty);
  }
  return (c == 'y' || c == 'Y');
}

static int copy_file(const char *src, const char *dst,
                     const CpOptions *opts) {
  struct stat dst_exist_stat;
  int dst_exists = (stat(dst, &dst_exist_stat) == 0);

  /* no-clobber: skip if destination already exists */
  if (opts->is_no_clobber && dst_exists) {
    return 0;
  }

  /* update: skip if destination exists and is newer than source */
  if (opts->is_update && dst_exists) {
    struct stat src_stat;
    if (stat(src, &src_stat) == 0 &&
        src_stat.st_mtime <= dst_exist_stat.st_mtime) {
      return 0;
    }
  }

  /* interactive: prompt before overwrite */
  if (opts->is_interactive && dst_exists) {
    if (!prompt_overwrite(dst)) {
      return 0;
    }
  }

  if (opts->is_verbose) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)printf("'%s' -> '%s'\n", src, dst);
  }

  FILE *fsrc = fopen(src, "rb");
  if (fsrc == NULL) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "cp: %s: No such file or directory\n", src);
    return -1;
  }

  FILE *fdst = fopen(dst, "wb");
  /* force: if open fails, unlink and retry */
  if (fdst == NULL && opts->is_force) {
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)unlink(dst);
    fdst = fopen(dst, "wb");
  }
  if (fdst == NULL) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "cp: %s: Cannot create destination file\n", dst);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fsrc);
    return -1;
  }

  char buf[COPY_BUF_SIZE];
  size_t nread;
  // NOLINTNEXTLINE(clang-analyzer-unix.Stream)
  while ((nread = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
    if (fwrite(buf, 1, nread, fdst) != nread) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "cp: %s: Error writing to destination\n", dst);
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)fclose(fsrc);
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)fclose(fdst);
      return -1;
    }
  }
  if (ferror(fsrc)) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "cp: %s: Error reading from source\n", src);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fsrc);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fdst);
    return -1;
  }

  // NOLINTNEXTLINE(bugprone-unused-return-value)
  (void)fclose(fsrc);
  // NOLINTNEXTLINE(bugprone-unused-return-value)
  (void)fclose(fdst);
  return 0;
}

/**
 * copy_recursive - Recursively copy src to dst
 * @src: path to source file or directory
 * @dst: path to destination
 * @is_verbose: print progress messages if non-zero
 * @is_force: remove existing destination file if non-zero
 * @is_no_clobber: do not overwrite existing files if non-zero
 *
 * Returns: 0 on success, -1 on error
 */
// NOLINTNEXTLINE(misc-no-recursion)
static int copy_recursive(const char *src, const char *dst,
                          const CpOptions *opts) {
  struct stat src_stat;
  if (stat(src, &src_stat) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "cp: %s: No such file or directory\n", src);
    return -1;
  }

  /* Regular file: copy directly */
  if (S_ISREG(src_stat.st_mode)) {
    return copy_file(src, dst, opts);
  }

  /* Directory: create destination and recurse */
  if (S_ISDIR(src_stat.st_mode)) {
    struct stat dst_stat;
    int dst_exists = (stat(dst, &dst_stat) == 0);

    if (dst_exists && !S_ISDIR(dst_stat.st_mode)) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "cp: %s: Not a directory\n", dst);
      return -1;
    }

    /* Create destination directory if it does not exist */
    if (!dst_exists) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      if (mkdir(dst, DIR_MODE) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: Cannot create directory\n", dst);
        return -1;
      }
      if (opts->is_verbose) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)printf("'%s' -> '%s'\n", src, dst);
      }
    }

    DIR *dir = opendir(src);
    if (dir == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "cp: %s: Cannot open directory\n", src);
      return -1;
    }

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    struct dirent *entry;
    int ret = 0;
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }

      char src_path[4096];
      char dst_path[4096];
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

      if (copy_recursive(src_path, dst_path, opts) != 0) {
        ret = -1;
      }
    }
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)closedir(dir);
    return ret;
  }

  /* Not a regular file or directory */
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)fprintf(stderr, "cp: %s: Not a regular file or directory\n", src);
  return -1;
}

// NOLINTNEXTLINE(misc-use-internal-linkage)
void cp_command(gint argc, gchar **argv) {
  struct arg_lit *recursive_opt =
      arg_lit0("r", "recursive", "copy directories recursively");
  struct arg_lit *verbose_opt =
      arg_lit0("v", "verbose", "explain what is being done");
  struct arg_lit *force_opt =
      arg_lit0("f", "force", "remove existing destination file");
  struct arg_lit *no_clobber_opt =
      arg_lit0("n", "no-clobber", "do not overwrite existing files");
  struct arg_lit *interactive_opt =
      arg_lit0("i", "interactive", "prompt before overwrite");
  struct arg_lit *update_opt =
      arg_lit0("u", "update",
               "copy only when source is newer than destination");
  /* Collect all remaining positional arguments in one file arg group,
   * then split them into sources and destination manually. */
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "SOURCE... DEST", 2, MAX_FILES,
                "source(s) followed by destination");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {recursive_opt,  verbose_opt, force_opt,
                      no_clobber_opt, interactive_opt, update_opt,
                      files_arg,     end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  CpOptions opts = {0};

  opts.is_recursive = (recursive_opt->count > 0);
  opts.is_verbose = (verbose_opt->count > 0);
  opts.is_force = (force_opt->count > 0);
  opts.is_no_clobber = (no_clobber_opt->count > 0);
  opts.is_interactive = (interactive_opt->count > 0);
  opts.is_update = (update_opt->count > 0);
  /* no-clobber overrides force and interactive */
  if (opts.is_no_clobber) {
    opts.is_force = 0;
    opts.is_interactive = 0;
  }
  int num_files = files_arg->count;

  if (num_files < 2) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "cp: missing destination operand\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  /* Last argument is destination, all preceding are sources */
  const char *dst = files_arg->filename[num_files - 1];
  int num_srcs = num_files - 1;
  int ret = 0;

  if (!opts.is_recursive) {
    /* Non-recursive mode: only regular files, single source */
    if (num_srcs != 1) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr,
                    "cp: expected one source file (use -r for recursive)\n");
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    const char *src = files_arg->filename[0];

    /* Check source exists and is a regular file */
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "cp: %s: No such file or directory\n", src);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    if (!S_ISREG(src_stat.st_mode)) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "cp: %s: Is not a regular file\n", src);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    /* Check if destination is an existing directory */
    struct stat dst_stat;
    gchar dest_path[4096];
    if (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
      /* Copy into directory: dest/basename(src) */
      gchar *basename = g_path_get_basename(src);
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
      g_free(basename);
    } else {
      /* Copy to the given path */
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
    }

    if (copy_file(src, dest_path, &opts) != 0) {
      ret = -1;
    }
  } else {
    /* Recursive mode */
    struct stat dst_stat;
    int dst_is_dir = (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode));

    if (num_srcs > 1 && !dst_is_dir) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "cp: target '%s' is not a directory\n", dst);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }

    for (int i = 0; i < num_srcs; i++) {
      const char *src = files_arg->filename[i];

      struct stat src_stat;
      if (stat(src, &src_stat) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: No such file or directory\n", src);
        continue;
      }

      gchar dest_path[4096];
      if (num_srcs > 1 || dst_is_dir) {
        /* Copy into directory: dest/basename(src) */
        gchar *basename = g_path_get_basename(src);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
        g_free(basename);
      } else {
        /* Copy to the given path */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
      }

      if (copy_recursive(src, dest_path, &opts) != 0) {
        ret = -1;
      }
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
  (void)ret; /* result tracked for future expansion */
}
