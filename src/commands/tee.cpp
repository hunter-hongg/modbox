#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <string>
#include <argtable3.h>

#include "commands/tee.hpp"

/* ── File management ─────────────────────────────────────────────────────── */

struct OutputFile {
    FILE* fp;
    const char* filename;
    int is_stdout;
};

static FILE* open_file(const char* filename, int append) {
    const char* mode = append ? "a" : "w";
    FILE* fp = fopen(filename, mode);
    if (fp == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "tee: %s: %s\n", filename, strerror(errno));
    }
    return fp;
}

/* ── Write to all outputs ────────────────────────────────────────────────── */

static int write_to_outputs(const std::vector<OutputFile>& outputs, 
                           const char* buf, size_t size, 
                           const TeeOptions* opts) {
    int any_error = 0;
    
    for (const auto& out : outputs) {
        if (out.is_stdout) {
            size_t written = fwrite(buf, 1, size, stdout);
            if (written != size) {
                if (opts->error_action == 0) { // warn
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "tee: stdout: write error\n");
                }
                any_error = 1;
            }
        } else {
            size_t written = fwrite(buf, 1, size, out.fp);
            if (written != size) {
                if (opts->error_action == 0) { // warn
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "tee: %s: write error\n", out.filename);
                } else if (opts->error_action == 1) { // warn-nopipe
                    if (errno != EPIPE) {
                        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                        (void)fprintf(stderr, "tee: %s: write error\n", out.filename);
                    }
                }
                // error_action == 2: ignore silently
                any_error = 1;
            }
        }
    }
    
    return any_error;
}

/* ── Main command ─────────────────────────────────────────────────────────── */

void tee_command(int argc, char** argv) {
    TeeOptions opts = {0};
    
    struct arg_lit* append_opt = arg_lit0("a", "append", "append to the given FILEs");
    struct arg_lit* ignore_int_opt = arg_lit0("i", "ignore-interrupts", "ignore interrupt signals");
    struct arg_str* error_action_opt = arg_str0("p", "error-action", "MODE", "what to do on write errors (warn, warn-nopipe, ignore)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "output file(s)");
    struct arg_end* end = arg_end(20);
    
    void* argtable[] = {
        append_opt, ignore_int_opt, error_action_opt, help_opt, file_arg, end
    };
    
    int nerrors = arg_parse(argc, argv, argtable);
    
    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Copy standard input to each FILE, and also to standard output.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -a, --append           append to the given FILEs, do not overwrite\n");
        printf("  -i, --ignore-interrupts ignore interrupt signals\n");
        printf("  -p, --error-action=MODE what to do on write errors\n");
        printf("                           warn (default), warn-nopipe, ignore\n");
        printf("  -h, --help             display this help and exit\n");
        printf("\n");
        printf("If a FILE is -, copy again to standard output.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }
    
    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }
    
    opts.append = (append_opt->count > 0);
    opts.ignore_interrupts = (ignore_int_opt->count > 0);
    
    if (error_action_opt->count > 0) {
        const char* mode = error_action_opt->sval[0];
        if (strcmp(mode, "warn") == 0) {
            opts.error_action = 0;
        } else if (strcmp(mode, "warn-nopipe") == 0) {
            opts.error_action = 1;
        } else if (strcmp(mode, "ignore") == 0) {
            opts.error_action = 2;
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "tee: invalid error action '%s'\n", mode);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }
    
    // Set up output files
    std::vector<OutputFile> outputs;
    
    // Always include stdout
    outputs.push_back({stdout, "stdout", 1});
    
    // Open specified files
    for (int i = 0; i < file_arg->count; i++) {
        const char* filename = file_arg->filename[i];
        if (strcmp(filename, "-") == 0) {
            // "-" means stdout, but we already have it
            continue;
        }
        
        FILE* fp = open_file(filename, opts.append);
        if (fp != NULL) {
            outputs.push_back({fp, filename, 0});
        }
    }
    
    // Read from stdin and write to all outputs
    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        size_t len = strlen(buf);
        write_to_outputs(outputs, buf, len, &opts);
    }
    
    // Handle potential binary data or last line without newline
    if (!feof(stdin)) {
        // Clear any error and try to read remaining
        clearerr(stdin);
        size_t bytes_read;
        while ((bytes_read = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            write_to_outputs(outputs, buf, bytes_read, &opts);
        }
    }
    
    // Close all file outputs (not stdout)
    for (auto& out : outputs) {
        if (!out.is_stdout && out.fp != NULL) {
            // NOLINTNEXTLINE(bugprone-unused-return-value)
            (void)fclose(out.fp);
        }
    }
    
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
