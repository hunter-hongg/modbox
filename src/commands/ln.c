#include <argtable3.h>
#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * do_link - Create a hard link from src to dst
 * @src: path to existing source file
 * @dst: path to destination link
 * @is_force: if set, remove dst before linking (when possible)
 *
 * Returns: 0 on success, -1 on error
 */
static int do_link(const char* src, const char* dst, int is_force) {
    if (is_force && (access(dst, F_OK) == 0)) {
        if (unlink(dst) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "ln: failed to remove '%s'\n", dst);
            return -1;
        }
    }

    if (link(src, dst) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "ln: failed to create link '%s' -> '%s'\n", dst, src);
        return -1;
    }

    return 0;
}

// NOLINTNEXTLINE(misc-use-internal-linkage)
void ln_command(gint argc, gchar** argv) {
    struct arg_lit* verbose_opt =
        arg_lit0("v", "verbose", "explain what is being done");
    struct arg_lit* force_opt =
        arg_lit0("f", "force", "remove existing destination files");
    struct arg_file* src_arg =
        arg_file1(NULL, NULL, "SOURCE", "source file to link to");
    struct arg_file* dst_arg =
        arg_filen(NULL, NULL, "DEST", 1, 1, "destination file or directory");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose_opt, force_opt, src_arg, dst_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int is_verbose = (verbose_opt->count > 0);
    int is_force = (force_opt->count > 0);
    const char* src = src_arg->filename[0];
    const char* dst = dst_arg->filename[0];

    /* Check that source exists and is a regular file */
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

    /* Determine the actual destination path */
    gchar dest_path[4096];
    struct stat dst_stat;
    if (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
        /* Destination is an existing directory: create link with source's basename inside */
        gchar* basename = g_path_get_basename(src);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
        g_free(basename);
    } else {
        /* Destination is a file path (existing or new): link directly */
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
    }

    if (do_link(src, dest_path, is_force) == 0) {
        if (is_verbose) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)printf("'%s' -> '%s'\n", dest_path, src);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

