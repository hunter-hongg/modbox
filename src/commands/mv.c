#include <argtable3.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Buffer size for file copy operations (cross-filesystem fallback) */
#define COPY_BUF_SIZE 8192

/* Default permissions for created directories */
#define DIR_MODE 0755

/* Maximum number of file arguments (sources + dest) */
#define MAX_FILES 101

/**
 * remove_recursive - Recursively remove a file or directory tree.
 *
 * Returns: 0 on success, -1 on error
 */
// NOLINTNEXTLINE(misc-no-recursion)
static int remove_recursive(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: cannot stat '%s': %s\n", path,
                  strerror(errno));
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: cannot open directory '%s': %s\n", path,
                    strerror(errno));
      return -1;
    }

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    struct dirent *entry;
    int ret = 0;
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }

      char child_path[4096];
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(child_path, sizeof(child_path), "%s/%s", path,
                     entry->d_name);

      if (remove_recursive(child_path) != 0) {
        ret = -1;
      }
    }
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)closedir(dir);

    if (rmdir(path) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: cannot remove directory '%s': %s\n", path,
                    strerror(errno));
      return -1;
    }
    return ret;
  }

  /* Regular file or other: unlink */
  if (unlink(path) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: cannot remove '%s': %s\n", path,
                  strerror(errno));
    return -1;
  }
  return 0;
}

/**
 * copy_recursive_for_mv - Recursively copy src to dst (for cross-filesystem
 * move).
 *
 * Returns: 0 on success, -1 on error
 */
// NOLINTNEXTLINE(misc-no-recursion)
static int copy_recursive_for_mv(const char *src, const char *dst) {
  struct stat src_stat;
  if (stat(src, &src_stat) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: %s: No such file or directory\n", src);
    return -1;
  }

  /* Regular file: copy directly */
  if (S_ISREG(src_stat.st_mode)) {
    FILE *fsrc = fopen(src, "rb");
    if (fsrc == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: %s: Cannot open for reading\n", src);
      return -1;
    }

    FILE *fdst = fopen(dst, "wb");
    if (fdst == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: %s: Cannot create destination file\n", dst);
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)fclose(fsrc);
      return -1;
    }

    char buf[COPY_BUF_SIZE];
    size_t nread;
    // NOLINTNEXTLINE(clang-analyzer-unix.Stream)
    while ((nread = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
      size_t nwritten = fwrite(buf, 1, nread, fdst);
      (void)nwritten;
    }

    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fsrc);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fdst);
    return 0;
  }

  /* Directory: create destination and recurse */
  if (S_ISDIR(src_stat.st_mode)) {
    struct stat dst_stat;
    int dst_exists = (stat(dst, &dst_stat) == 0);

    if (dst_exists && !S_ISDIR(dst_stat.st_mode)) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: %s: Not a directory\n", dst);
      return -1;
    }

    if (!dst_exists) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      if (mkdir(dst, DIR_MODE) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "mv: %s: Cannot create directory\n", dst);
        return -1;
      }
    }

    DIR *dir = opendir(src);
    if (dir == NULL) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: %s: Cannot open directory\n", src);
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

      if (copy_recursive_for_mv(src_path, dst_path) != 0) {
        ret = -1;
      }
    }
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)closedir(dir);
    return ret;
  }

  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)fprintf(stderr, "mv: %s: Not a regular file or directory\n", src);
  return -1;
}

/**
 * move_entry - Move/rename a file or directory from src to dst.
 * Uses rename() for same-filesystem moves; falls back to copy+remove
 * for cross-filesystem moves.
 *
 * Returns: 0 on success, -1 on error
 */
static int move_entry(const char *src, const char *dst) {
  /* Quick check: if source does not exist, report error */
  struct stat src_stat;
  if (stat(src, &src_stat) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: %s: No such file or directory\n", src);
    return -1;
  }

  /* Try rename() first (fast path, same filesystem) */
  if (rename(src, dst) == 0) {
    return 0;
  }

  /* Cross-filesystem: fall back to copy + remove */
  // NOLINTNEXTLINE(misc-include-cleaner)
  if (errno == EXDEV) {
    if (copy_recursive_for_mv(src, dst) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: failed to copy '%s' to '%s'\n", src, dst);
      return -1;
    }
    if (remove_recursive(src) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: failed to remove source '%s' after copy\n",
                    src);
      return -1;
    }
    return 0;
  }

  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)fprintf(stderr, "mv: cannot move '%s' to '%s': %s\n", src, dst,
                strerror(errno));
  return -1;
}

// NOLINTNEXTLINE(misc-use-internal-linkage)
void mv_command(gint argc, gchar **argv) {
  /* Collect all remaining positional arguments in one file arg group,
   * then split them into sources and destination manually. */
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "SOURCE... DEST", 2, MAX_FILES,
                "source(s) followed by destination");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {files_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  int num_files = files_arg->count;

  if (num_files < 2) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: missing destination operand\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  /* Last argument is destination, all preceding are sources */
  const char *dst = files_arg->filename[num_files - 1];
  int num_srcs = num_files - 1;

  /* Check if destination is an existing directory */
  struct stat dst_stat;
  int dst_is_dir = (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode));

  /* Multiple sources: destination must be an existing directory */
  if (num_srcs > 1 && !dst_is_dir) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: target '%s' is not a directory\n", dst);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  for (int i = 0; i < num_srcs; i++) {
    const char *src = files_arg->filename[i];

    /* Check source exists */
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: %s: No such file or directory\n", src);
      continue;
    }

    /* Determine actual destination path */
    gchar dest_path[4096];
    if (dst_is_dir) {
      /* Destination is an existing directory: move into it */
      gchar *basename = g_path_get_basename(src);
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
      g_free(basename);
    } else {
      /* Destination is a path (existing file or new): rename/move to it */
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
    }

    /* Prevent moving a directory into itself */
    if (S_ISDIR(src_stat.st_mode) && dst_is_dir) {
      /* Check if dest_path is inside src (would be a recursive move) */
      gchar *resolved_src = realpath(src, NULL);
      gchar *resolved_dst = realpath(dest_path, NULL);
      if (resolved_src != NULL && resolved_dst != NULL) {
        size_t src_len = strlen(resolved_src);
        if (strncmp(resolved_src, resolved_dst, src_len) == 0 &&
            (resolved_dst[src_len] == '/' || resolved_dst[src_len] == '\0')) {
          // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
          (void)fprintf(
              stderr,
              "mv: cannot move '%s' to a subdirectory of itself, '%s'\n", src,
              dest_path);
          free(resolved_src);
          free(resolved_dst);
          continue;
        }
      }
      free(resolved_src);
      free(resolved_dst);
    }

    move_entry(src, dest_path);
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
