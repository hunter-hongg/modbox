#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/link.hpp"

void link_command(int argc, char** argv) {
    struct arg_lit* verbose_opt =
        arg_lit0("v", "verbose", "explain what is being done");
    struct arg_file* src_arg =
        arg_file1(NULL, NULL, "FILE", "existing file to link to");
    struct arg_file* dst_arg =
        arg_file1(NULL, NULL, "LINK_NAME", "name for the new link");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose_opt, src_arg, dst_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    LinkOptions opts = {};
    opts.is_verbose = (verbose_opt->count > 0);

    const char* src = src_arg->filename[0];
    const char* dst = dst_arg->filename[0];

    /* Check that source file exists */
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "link: failed to access '%s': %s\n", src, strerror(errno));
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    /* Check that destination directory exists */
    const char* last_slash = strrchr(dst, '/');
    if (last_slash != NULL) {
        /* Extract directory path */
        size_t dir_len = last_slash - dst;
        char dst_dir[4096];
        if (dir_len >= sizeof(dst_dir)) {
            dir_len = sizeof(dst_dir) - 1;
        }
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)strncpy(dst_dir, dst, dir_len);
        dst_dir[dir_len] = '\0';

        struct stat dst_dir_stat;
        if (stat(dst_dir, &dst_dir_stat) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "link: failed to access directory '%s': %s\n", dst_dir, strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        if (!S_ISDIR(dst_dir_stat.st_mode)) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "link: '%s' is not a directory\n", dst_dir);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    } else {
        /* Destination is in current directory, which should exist */
        struct stat cwd_stat;
        if (stat(".", &cwd_stat) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "link: failed to access current directory: %s\n", strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    /* Create the hard link */
    if (link(src, dst) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "link: failed to create link '%s' to '%s': %s\n",
                      dst, src, strerror(errno));
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (opts.is_verbose) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)printf("'%s' linked to '%s'\n", dst, src);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
