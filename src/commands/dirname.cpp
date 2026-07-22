#include <cstdio>
#include <cstring>
#include <string>

#include "commands/dirname.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s NAME\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("  or:  %s NAME...\n", prog);
    printf("Print NAME with its trailing /component removed.\n");
    printf("If NAME contains no /, print '.'.\n");
    printf("\n");
    printf("  -z, --zero        end each output line with NUL, not newline\n");
    printf("      --help        display this help and exit\n");
    printf("      --version     output version information and exit\n");
}

static std::string dirname_of(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    // Strip trailing slashes (but not the root slash)
    std::string p = path;
    size_t end = p.size();
    while (end > 1 && p[end - 1] == '/') {
        end--;
    }
    p = p.substr(0, end);

    if (p.empty()) {
        return "/";
    }

    // Find last non-trailing slash
    size_t slash = p.rfind('/');
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return "/";
    }
    return p.substr(0, slash);
}

void dirname_command(int argc, char** argv) {
    bool zero = false;
    const char* prog = argv[0];
    int name_start = 1;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("dirname (modbox) 1.0\n");
            return;
        }
        if (strcmp(a, "-z") == 0 || strcmp(a, "--zero") == 0) {
            zero = true;
            name_start = i + 1;
            continue;
        }
        if (a[0] == '-' && a[1] != '\0') {
            (void)fprintf(stderr, "%s: unrecognized option '%s'\n", prog, a);
            (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
            return;
        }
    }

    if (name_start >= argc) {
        (void)fprintf(stderr, "%s: missing operand\n", prog);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return;
    }

    for (int i = name_start; i < argc; i++) {
        std::string d = dirname_of(argv[i]);
        if (zero) {
            printf("%s", d.c_str());
        } else {
            printf("%s\n", d.c_str());
        }
    }
}

REGISTER_COMMAND("dirname", dirname_command, "Strip non-directory suffix");
