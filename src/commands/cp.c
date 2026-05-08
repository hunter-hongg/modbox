#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <glib.h>
#include <argtable3.h>

/* Buffer size for file copy operations */
#define COPY_BUF_SIZE 8192

/**
 * copy_file - Copy contents of src file to dst file
 * @src: path to source file
 * @dst: path to destination file
 *
 * Returns: 0 on success, -1 on error
 */
static int copy_file(const char* src, const char* dst) {
    FILE* fsrc = fopen(src, "rb");
    if (fsrc == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: No such file or directory\n", src);
        return -1;
    }

    FILE* fdst = fopen(dst, "wb");
    if (fdst == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: Cannot create destination file\n", dst);
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        (void)fclose(fsrc);
        return -1;
    }

    char buf[COPY_BUF_SIZE];
    size_t nread;
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

// NOLINTNEXTLINE(misc-use-internal-linkage)
void cp_command(gint argc, gchar** argv) {
    struct arg_file* src_arg = arg_filen(NULL, NULL, "SOURCE", 1, 1, "source file");
    struct arg_file* dest_arg = arg_filen(NULL, NULL, "DEST", 1, 1, "destination path");
    struct arg_end* end = arg_end(20);

    void* argtable[] = { src_arg, dest_arg, end };

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    const char* src = src_arg->filename[0];
    const char* dst = dest_arg->filename[0];

    // Check source exists and is a regular file
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

    // Check if destination is an existing directory
    struct stat dst_stat;
    gchar dest_path[4096];
    if (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
        // Copy into directory: dest/basename(src)
        gchar* basename = g_path_get_basename(src);
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", dst, basename);
        g_free(basename);
    } else {
        // Copy to the given path
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(dest_path, sizeof(dest_path), "%s", dst);
    }

    copy_file(src, dest_path);

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

