#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands/basename.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s NAME [SUFFIX]\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("  or:  %s -a NAME... [-s SUFFIX]\n", prog);
    printf("Print NAME with any leading directory components removed.\n");
    printf("If SUFFIX is specified, also remove a trailing SUFFIX.\n");
    printf("\n");
    printf("  -a, --multiple    support multiple arguments and treat each as a NAME\n");
    printf("  -s, --suffix=SUFFIX  remove a trailing SUFFIX (implies -a)\n");
    printf("  -z, --zero        end each output line with NUL, not newline\n");
    printf("      --help        display this help and exit\n");
    printf("      --version     output version information and exit\n");
}

static std::string strip_dir(const char* name) {
    // Strip trailing slashes (but keep the root slash)
    size_t len = strlen(name);
    while (len > 1 && name[len - 1] == '/') {
        len--;
    }

    // Scan back for the last separator
    for (size_t i = len; i > 0; i--) {
        if (name[i - 1] == '/') {
            return std::string(name + i, len - i);
        }
    }
    return std::string(name, len);
}

static std::string strip_suffix(const std::string& name, const std::string& suffix) {
    if (suffix.empty()) {
        return name;
    }
    if (name.size() >= suffix.size() &&
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return name.substr(0, name.size() - suffix.size());
    }
    return name;
}

void basename_command(int argc, char** argv) {
    bool multiple = false;
    std::string suffix;
    bool zero = false;
    std::vector<const char*> names;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            printf("basename (modbox) 1.0\n");
            return;
        }
        if (strcmp(a, "-a") == 0 || strcmp(a, "--multiple") == 0) {
            multiple = true;
            continue;
        }
        if (strcmp(a, "-z") == 0 || strcmp(a, "--zero") == 0) {
            zero = true;
            continue;
        }
        if (strncmp(a, "-s", 2) == 0) {
            if (a[2] != '\0') {
                suffix = a + 2;
            } else {
                i++;
                if (i < argc) {
                    suffix = argv[i];
                }
            }
            multiple = true;
            continue;
        }
        if (strcmp(a, "--suffix") == 0) {
            i++;
            if (i < argc) {
                suffix = argv[i];
            }
            multiple = true;
            continue;
        }
        // Skip option-like arguments that aren't recognized
        if (a[0] == '-' && a[1] != '\0') {
            (void)fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], a);
            (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return;
        }
        names.push_back(a);
    }

    if (names.empty()) {
        (void)fprintf(stderr, "%s: missing operand\n", argv[0]);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return;
    }

    if (!multiple) {
        // Single NAME [SUFFIX] mode
        std::string result = strip_dir(names[0]);
        if (names.size() > 1) {
            result = strip_suffix(result, names[1]);
        } else if (!suffix.empty()) {
            result = strip_suffix(result, suffix);
        }
        if (zero) {
            printf("%s", result.c_str());
        } else {
            printf("%s\n", result.c_str());
        }
    } else {
        // Multiple NAMEs mode
        for (size_t i = 0; i < names.size(); i++) {
            std::string result = strip_suffix(strip_dir(names[i]), suffix);
            if (zero) {
                printf("%s", result.c_str());
            } else {
                printf("%s\n", result.c_str());
            }
        }
    }
}
