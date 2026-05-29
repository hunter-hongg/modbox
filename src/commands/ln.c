#include <argtable3.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/ln.h"

#define MAX_PATH_LEN 4096

/**
 * do_link - Create a hard link or symbolic link from src to dst
 * @src: path to source file (for symlinks, need not exist)
 * @dst: path to destination link
 * @is_force: if set, remove dst before linking (when possible)
 * @is_sym: if set, create a symbolic link instead of a hard link
 *
 * Returns: 0 on success, -1 on error
 */
static int do_link(const char* src, const char* dst, const LnOptions* opts) {
    if (opts->is_force) {
        /* Attempt to remove any existing destination; ignore ENOENT */
        // NOLINTNEXTLINE(misc-include-cleaner)
        if (unlink(dst) != 0 && errno != ENOENT) {
            // NOLINTNEXTLINE(misc-include-cleaner)
            if (errno == EISDIR) {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "ln: cannot remove '%s': Is a directory\n", dst);
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stderr, "ln: failed to remove '%s': %s\n", dst, strerror(errno));
            }
            return -1;
        }
    }

    if (opts->is_sym) {
        if (symlink(src, dst) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "ln: failed to create symbolic link '%s' -> '%s': %s\n",
                          dst, src, strerror(errno));
            return -1;
        }
    } else {
        if (link(src, dst) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "ln: failed to create link '%s' -> '%s': %s\n",
                          dst, src, strerror(errno));
            return -1;
        }
    }

    return 0;
}

// NOLINTNEXTLINE(misc-use-internal-linkage)
void ln_command(gint argc, gchar** argv) {
    struct arg_lit* verbose_opt =
        arg_lit0("v", "verbose", "explain what is being done");
    struct arg_lit* force_opt =
        arg_lit0("f", "force", "remove existing destination files");
    struct arg_lit* symbolic_opt =
        arg_lit0("s", "symbolic", "make symbolic links instead of hard links");
    struct arg_lit* interactive_opt =
        arg_lit0("i", "interactive", "prompt before overwrite");
    struct arg_lit* noderef_opt =
        arg_lit0("n", "no-dereference", "do not dereference destination if it is a symlink");
    struct arg_file* src_arg =
        arg_file1(NULL, NULL, "SOURCE", "source file to link to");
    struct arg_file* dst_arg =
        arg_filen(NULL, NULL, "DEST", 1, 1, "destination file or directory");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose_opt, force_opt, symbolic_opt, interactive_opt, noderef_opt, src_arg, dst_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    LnOptions opts = {0};

    opts.is_verbose = (verbose_opt->count > 0);
    opts.is_force = (force_opt->count > 0);
    opts.is_sym = (symbolic_opt->count > 0);
    opts.is_interactive = (interactive_opt->count > 0);
    opts.is_no_deref = (noderef_opt->count > 0);
    const char* src = src_arg->filename[0];
    const char* dst = dst_arg->filename[0];

    /* For hard links, source must exist and be a regular file */
    if (!opts.is_sym) {
        struct stat src_stat;
        if (stat(src, &src_stat) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "ln: %s: No such file or directory\n", src);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        if (!S_ISREG(src_stat.st_mode)) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "ln: %s: Is not a regular file\n", src);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    /* Determine the actual destination path */
    gchar dest_path[MAX_PATH_LEN];
    struct stat dst_stat;
    struct stat ldst_stat;
    int dst_is_dir = 0;
    if (lstat(dst, &ldst_stat) == 0) {
        if (S_ISLNK(ldst_stat.st_mode) && opts.is_no_deref) {
            dst_is_dir = 0; /* respect -n: do not follow symlink */
        } else {
            /* follow symlink or normal file */
            if (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
                dst_is_dir = 1;
            }
        }
    } else {
        /* lstat failed (dst likely doesn't exist) */
        dst_is_dir = 0;
    }

    if (dst_is_dir) {
        gchar* basename = g_path_get_basename(src);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
        g_free(basename);
    } else {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
    }

    /* Interactive prompt: if -i and the destination exists, ask before proceeding */
    if (opts.is_interactive) {
        struct stat exist_stat;
        if (lstat(dest_path, &exist_stat) == 0) {
            /* Destination exists; prompt user */
            // NOLINTNEXTLINE(misc-include-cleaner)
            FILE* tty = fopen("/dev/tty", "r+");
            if (tty == NULL) {
                /* Fallback to stdin/stdout */
                tty = stdin;
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(stdout, "replace '%s'? [y/N] ", dest_path);
            } else {
                // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                (void)fprintf(tty, "replace '%s'? [y/N] ", dest_path);
            }
            int c = fgetc(tty);
            /* Consume rest of line */
            if (c != '\n' && c != EOF) {
                int ch;
                do { ch = fgetc(tty); } while (ch != '\n' && ch != EOF);
            }
            if (tty != stdin) {
                // NOLINTNEXTLINE(bugprone-unused-return-value)
                (void)fclose(tty);
            }
            if (c != 'y' && c != 'Y') {
                arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                return;
            }
        }
    }

    if (do_link(src, dest_path, &opts) == 0) {
        if (opts.is_verbose) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)printf("'%s' -> '%s'\n", dest_path, src);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
