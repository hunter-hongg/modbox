#include <argtable3.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "commands/rm.hpp"
#include "commands/command_macros.hpp"

#define TRASH_DIR ".trash"

/* ── Trash helpers ──────────────────────────────────────────────────── */

static int ensure_trash_dir(const char *trash_dir) {
  struct stat st;
  if (stat(trash_dir, &st) == 0) {
    if (S_ISDIR(st.st_mode)) return 0;
    errno = ENOTDIR;
    return -1;
  }
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  return mkdir(trash_dir, 0700);
}

static char* resolve_trash_path(const char* path, const char* home) {
    static char dest[4096];
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(dest, sizeof(dest), "%s/.trash/%s", home, base);
    if (access(dest, F_OK) != 0) return dest;
    for (int i = 1; i < 10000; i++) {
        snprintf(dest, sizeof(dest), "%s/.trash/%s.%d", home, base, i);
        if (access(dest, F_OK) != 0) return dest;
    }
    snprintf(dest, sizeof(dest), "%s/.trash/%s.%ld.%d", home, base, (long)time(NULL), rand());
    return dest;
}

static int copy_file_to_trash(const char *src, const char *dest) {
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  FILE *fsrc = fopen(src, "rb");
  if (fsrc == NULL) return -1;

  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  FILE *fdst = fopen(dest, "wb");
  if (fdst == NULL) {
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fsrc);
    return -1;
  }

  char buf[8192];
  size_t nread;
  // NOLINTNEXTLINE(clang-analyzer-unix.Stream)
  while ((nread = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
    if (fwrite(buf, 1, nread, fdst) != nread) {
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)fclose(fsrc);
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)fclose(fdst);
      return -1;
    }
  }

  if (ferror(fsrc)) {
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fsrc);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fclose(fdst);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)unlink(dest);
    return -1;
  }

  // NOLINTNEXTLINE(bugprone-unused-return-value)
  (void)fclose(fsrc);
  // NOLINTNEXTLINE(bugprone-unused-return-value)
  (void)fclose(fdst);
  return 0;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int move_to_trash(const char *path, const RmOptions *opts) {
  struct stat st;
  if (lstat(path, &st) != 0) return -1;

  const char *home = getenv("HOME");
  if (home == NULL) return -1;

  char trash_dir[4096];
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  (void)snprintf(trash_dir, sizeof(trash_dir), "%s/%s", home, TRASH_DIR);
  if (ensure_trash_dir(trash_dir) != 0) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "rm: cannot create trash directory '%s': %s\n",
                  trash_dir, strerror(errno));
    return -1;
  }

  char *dest = resolve_trash_path(path, home);
  if (dest == NULL) return -1;

  int ret = 0;

  if (S_ISDIR(st.st_mode)) {
    /* Try rename first (same filesystem) */
    ret = rename(path, dest);
    if (ret != 0 && errno == EXDEV) {
      /* Cross-filesystem: create dest dir, recurse */
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      if (mkdir(dest, 0700) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "rm: cannot trash '%s': %s\n", path,
                      strerror(errno));
        return -1;
      }

      DIR *dir = opendir(path);
      if (dir == NULL) {
        return -1;
      }

      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
          continue;
        }
        char child_src[4096];
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(child_src, sizeof(child_src), "%s/%s", path, entry->d_name);
        if (move_to_trash(child_src, opts) != 0) ret = -1;
      }
      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)closedir(dir);

      // NOLINTNEXTLINE(bugprone-unused-return-value)
      (void)rmdir(path);
    }
  } else {
    /* Regular file or symlink */
    ret = rename(path, dest);
    if (ret != 0 && errno == EXDEV) {
      /* Cross-filesystem: copy then unlink */
      if (copy_file_to_trash(path, dest) == 0) {
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)unlink(path);
        ret = 0;
      } else {
        ret = -1;
      }
    }
  }

  if (ret == 0 && opts->is_verbose) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)printf("trashed '%s' -> '%s'\n", path, dest);
  }

  return ret;
}

/* ── Prompt helpers ─────────────────────────────────────────────────── */

static bool prompt_remove(const char *path) {
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
    (void)fprintf(out, "rm: remove '%s'? ", path);
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)fflush(out);
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

// NOLINTNEXTLINE(misc-no-recursion)
static int remove_entry(const char *path, const RmOptions *opts) {
  /* Trash mode: move to ~/.trash instead of unlinking */
  if (opts->is_trash) {
    return move_to_trash(path, opts);
  }

  struct stat st;
  if (lstat(path, &st) != 0) return -1;

  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path);
    if (dir == NULL) return -1;

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    struct dirent *entry;
    int ret = 0;
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }

      char child[4096];
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
      if (remove_entry(child, opts) != 0) {
        ret = -1;
      }
    }
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)closedir(dir);

    if (rmdir(path) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "rm: cannot remove '%s': %s\n", path,
                    strerror(errno));
      return -1;
    }
    return ret;
  }

  if (unlink(path) != 0) {
    return -1;
  }
  return 0;
}

static int remove_file(const char *path, const RmOptions *opts) {
  struct stat st;

  /* -d: only remove with rmdir if it's an empty directory */
  if (opts->remove_empty_dirs) {
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      if (rmdir(path) == 0) {
        return 0;
      }
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "rm: cannot remove '%s': %s\n", path,
                    strerror(errno));
      return -1;
    }
  }

  if (lstat(path, &st) != 0) {
    return -1;
  }

  if (S_ISDIR(st.st_mode) && !opts->is_recursive) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", path);
    return -1;
  }

    /* Interactive prompt */
  if (opts->is_interactive) {
    if (!prompt_remove(path)) {
      return 0;
    }
  }

  if (S_ISDIR(st.st_mode)) {
    if (remove_entry(path, opts) != 0) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "rm: cannot remove '%s': %s\n", path,
                    strerror(errno));
      return -1;
    }
  } else {
    if (opts->is_trash) {
      if (move_to_trash(path, opts) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "rm: cannot trash '%s': %s\n", path,
                      strerror(errno));
        return -1;
      }
    } else {
      if (unlink(path) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "rm: cannot remove '%s': %s\n", path,
                      strerror(errno));
        return -1;
      }
    }
  }

  if (opts->is_verbose) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)printf("removed '%s'\n", path);
  }

  return 0;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void rm_command(int argc, char **argv) {
  struct arg_lit *recursive_opt =
      arg_lit0("r", "recursive", "remove directories and their contents recursively");
  struct arg_lit *force_opt =
      arg_lit0("f", "force", "ignore nonexistent files and arguments, never prompt");
  struct arg_lit *interactive_opt =
      arg_lit0("i", "interactive", "prompt before every removal");
  struct arg_lit *verbose_opt =
      arg_lit0("v", "verbose", "explain what is being done");
  struct arg_lit *dir_opt =
      arg_lit0("d", "dir", "remove empty directories");
  struct arg_lit *one_file_system_opt =
      arg_lit0(NULL, "one-file-system",
               "when removing a hierarchy recursively, skip any directory that "
               "is on a file system different from that of the corresponding "
               "command line argument");
  struct arg_lit *no_preserve_root_opt =
      arg_lit0(NULL, "no-preserve-root", "do not treat '/' specially");
  struct arg_lit *preserve_root_opt =
      arg_lit0(NULL, "preserve-root", "do not remove '/' (default)");
  struct arg_lit *trash_opt =
      arg_lit0(NULL, "trash", "move files to ~/.trash instead of deleting");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *files_arg =
      arg_filen(NULL, NULL, "FILE...", 1, 1000, "files or directories to remove");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {recursive_opt, force_opt, interactive_opt,
                      verbose_opt, dir_opt,
                      one_file_system_opt, no_preserve_root_opt,
                      preserve_root_opt, trash_opt, help_opt,
                      files_arg, end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... FILE...\n", argv[0]);
    printf("Remove (unlink) the FILE(s).\n");
    printf("\n");
    printf("  -d, --dir           remove empty directories\n");
    printf("  -f, --force         ignore nonexistent files and arguments, never prompt\n");
    printf("  -i, --interactive   prompt before every removal\n");
    printf("  -r, --recursive     remove directories and their contents recursively\n");
    printf("  -v, --verbose       explain what is being done\n");
    printf("      --one-file-system  skip directories on different file systems\n");
    printf("      --no-preserve-root  do not treat '/' specially\n");
    printf("      --preserve-root    do not remove '/' (default)\n");
    printf("      --trash         move files to ~/.trash instead of deleting\n");
    printf("  -h, --help          display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  RmOptions opts = {};
  opts.is_recursive = (recursive_opt->count > 0);
  opts.is_force = (force_opt->count > 0);
  opts.is_interactive = (interactive_opt->count > 0);
  opts.is_verbose = (verbose_opt->count > 0);
  opts.remove_empty_dirs = (dir_opt->count > 0);
  opts.is_trash = (trash_opt->count > 0);

  /* -f overrides -i */
  if (opts.is_force) {
    opts.is_interactive = 0;
  }

  /* Default: no prompting — use -i for interactive confirmation */

  int num_files = files_arg->count;

  if (num_files < 1) {
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr, "rm: missing operand\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  for (int i = 0; i < num_files; i++) {
    const char *path = files_arg->filename[i];

    /* If force, skip nonexistent files silently */
    if (opts.is_force) {
      struct stat st;
      if (lstat(path, &st) != 0) {
        if (errno == ENOENT) {
          continue;
        }
      }
    }

    if (remove_file(path, &opts) != 0) {
      /* Error already printed by remove_file */
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("rm", rm_command, "Remove files or directories");
