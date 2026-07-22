#include <argtable3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands/unlink.hpp"
#include "commands/command_macros.hpp"

void unlink_command(int argc, char** argv) {
    struct arg_lit* verbose_opt =
        arg_lit0("v", "verbose", "explain what is being done");
    struct arg_lit* help_opt =
        arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg =
        arg_file1(NULL, NULL, "FILE", "file to unlink");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose_opt, help_opt, file_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION] FILE\n", argv[0]);
        printf("Call the unlink function to remove the specified FILE.\n");
        printf("\n");
        printf("  -v, --verbose     explain what is being done\n");
        printf("  -h, --help        display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    UnlinkOptions opts = {};
    opts.is_verbose = (verbose_opt->count > 0);

    const char* filename = file_arg->filename[0];

    /* Call unlink to remove the file */
    if (unlink(filename) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "unlink: cannot unlink '%s': %s\n", filename, strerror(errno));
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (opts.is_verbose) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)printf("unlinked '%s'\n", filename);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("unlink", unlink_command, "Remove file");
