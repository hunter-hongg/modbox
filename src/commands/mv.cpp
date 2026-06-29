#include <argtable3.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/mv.hpp"

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
 * prompt_overwrite - Ask user whether to overwrite destination.
 *
 * Returns: TRUE if user says yes, FALSE otherwise
 */
static bool prompt_overwrite(const char *dst) {
  FILE *in = stdin;
  FILE *out = stdout;
  int in_opened = 0;
  int out_opened = 0;

  if (isatty(STDIN_FILENO)) {
    in = fopen("/dev/tty", "r");
    if (in != NULL) {
      in_opened = 1;
    }
  }
  if (isatty(STDOUT_FILENO)) {
    out = fopen("/dev/tty", "w");
    if (out != NULL) {
      out_opened = 1;
    }
  }

  if (out != NULL) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out, "mv: overwrite '%s'? ", dst);
  }

  int c = EOF;
  if (in != NULL) {
    c = fgetc(in);
    if (c != '\n' && c != EOF) {
      int ch;
      do { ch = fgetc(in); } while (ch != '\n' && ch != EOF);
    }
  }

  if (in_opened && in != NULL) {
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(in);
  }
  if (out_opened && out != NULL) {
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(out);
  }
  return (c == 'y' || c == 'Y');
}

/**
 * move_entry - Move/rename a file or directory from src to dst.
 * Uses rename() for same-filesystem moves; falls back to copy+remove
 * for cross-filesystem moves.
 *
 * Returns: 0 on success, -1 on error
 */
static int move_entry(const char *src, const char *dst, const MvOptions *opts) {
  /* Quick check: if source does not exist, report error */
  struct stat src_stat;
  if (stat(src, &src_stat) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "mv: %s: No such file or directory\n", src);
    return -1;
  }

  /* Check if destination exists */
  struct stat dst_stat;
  int dst_exists = (stat(dst, &dst_stat) == 0);

  /* -u (update): only move if SOURCE is newer than DEST or DEST missing */
  if (opts->is_update && dst_exists) {
    if (src_stat.st_mtime <= dst_stat.st_mtime) {
      return 0;
    }
  }

  if (dst_exists) {
    if (opts->is_no_clobber) {
      return 0;
    }
    /* -f (force) overrides -i (interactive) */
    if (opts->is_interactive && !opts->is_force) {
      if (!prompt_overwrite(dst)) {
        return 0;
      }
    }
    /* -b (backup): move existing destination to DEST~ */
    if (opts->is_backup) {
      char backup_path[4096];
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(backup_path, sizeof(backup_path), "%s~", dst);
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)rename(dst, backup_path);
    }
    /* -f (force): remove existing destination before rename */
    if (opts->is_force) {
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)unlink(dst);
    }
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

// NOLINTNEXTLINE(misc-use-internal-linkage,readability-function-cognitive-complexity)
void mv_command(int argc, char **argv) {
  struct arg_lit *interactive_opt =
      arg_lit0("i", "interactive", "prompt before overwrite");
  struct arg_lit *no_clobber_opt =
      arg_lit0("n", "no-clobber", "do not overwrite existing files");
  struct arg_lit *force_opt =
      arg_lit0("f", "force", "remove existing destination, never prompt");
  struct arg_lit *verbose_opt =
      arg_lit0("v", "verbose", "explain what is being done");
  struct arg_lit *update_opt =
      arg_lit0("u", "update", "move only when SOURCE is newer than DEST");
  struct arg_lit *backup_opt =
      arg_lit0("b", "backup", "back up existing destination files (append ~)");
  struct arg_str *target_dir_opt =
      arg_str0("t", "target-directory", "DIRECTORY",
               "move all sources into DIRECTORY");
  struct arg_lit *no_target_dir_opt =
      arg_lit0("T", "no-target-directory",
               "treat DEST as a normal file, not a directory");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "SOURCE... DEST", 0, MAX_FILES,
                "source(s) followed by destination");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {interactive_opt, no_clobber_opt, force_opt,
                      verbose_opt, update_opt, backup_opt,
                      target_dir_opt, no_target_dir_opt,
                      help_opt,
                      files_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... SOURCE DEST\n", argv[0]);
    printf("  or:  %s [OPTION]... SOURCE... DIRECTORY\n", argv[0]);
    printf("  or:  %s [OPTION]... -t DIRECTORY SOURCE...\n", argv[0]);
    printf("Move (rename) SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -f, --force            remove existing destination, never prompt\n");
    printf("  -i, --interactive      prompt before overwrite\n");
    printf("  -n, --no-clobber       do not overwrite existing files\n");
    printf("  -u, --update           move only when SOURCE is newer than DEST\n");
    printf("  -v, --verbose          explain what is being done\n");
    printf("  -b, --backup           back up existing destination files (append ~)\n");
    printf("  -t, --target-directory=DIR  move all sources into DIRECTORY\n");
    printf("  -T, --no-target-directory   treat DEST as a normal file\n");
    printf("  -h, --help             display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  MvOptions opts = {};
  opts.is_interactive = (interactive_opt->count > 0);
  opts.is_no_clobber = (no_clobber_opt->count > 0);
  opts.is_force = (force_opt->count > 0);
  opts.is_verbose = (verbose_opt->count > 0);
  opts.is_update = (update_opt->count > 0);
  opts.is_backup = (backup_opt->count > 0);
  opts.target_dir = (target_dir_opt->count > 0) ? target_dir_opt->sval[0] : NULL;
  opts.no_target_dir = (no_target_dir_opt->count > 0);

  /* no-clobber overrides interactive */
  if (opts.is_no_clobber) {
    opts.is_interactive = 0;
    opts.is_force = 0;
  }
  /* force overrides interactive */
  if (opts.is_force) {
    opts.is_interactive = 0;
  }

  int num_files = files_arg->count;
  const char *dst = NULL;
  int num_srcs = 0;

  if (opts.target_dir != NULL) {
    /* -t: all positional args are sources, dest from option */
    dst = opts.target_dir;

    /* Verify -t target is an existing directory */
    struct stat tgt_stat;
    if (stat(dst, &tgt_stat) != 0) {
      // NOLINTNEXTLINE(misc-include-cleaner)
      const char *msg = (errno == ENOENT)
          ? "No such file or directory" : "is not a directory";
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: target '%s': %s\n", dst, msg);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    if (!S_ISDIR(tgt_stat.st_mode)) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: target '%s' is not a directory\n", dst);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    num_srcs = num_files;
    if (num_srcs < 1) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: missing file operand\n");
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
  } else {
    if (num_files < 2) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: missing destination operand\n");
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    /* Last argument is destination, all preceding are sources */
    dst = files_arg->filename[num_files - 1];
    num_srcs = num_files - 1;
  }

  /* Check if destination is an existing directory */
  struct stat dst_stat;
  int dst_is_dir = (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode));

  /* -T: treat DEST as normal file, error if multiple sources */
  if (opts.no_target_dir) {
    if (num_srcs > 1) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "mv: extra operand '%s'\n",
                    files_arg->filename[1]);
      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return;
    }
    dst_is_dir = 0;
  }

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
    char dest_path[4096];
    if (dst_is_dir) {
      /* Destination is an existing directory: move into it */
      const char *basename = strrchr(src, '/');
      basename = basename ? basename + 1 : src;
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
    } else {
      /* Destination is a path (existing file or new): rename/move to it */
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
    }

    /* Prevent moving a directory into itself */
    if (S_ISDIR(src_stat.st_mode) && dst_is_dir) {
      char *resolved_src = realpath(src, NULL);
      char *resolved_dst = realpath(dest_path, NULL);
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

    if (move_entry(src, dest_path, &opts) == 0 && opts.is_verbose) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)printf("'%s' -> '%s'\n", src, dest_path);
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
