#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include "commands/pathchk.hpp"
#include "commands/command_macros.hpp"

static constexpr size_t POSIX_NAME_MAX = 255;
static constexpr size_t POSIX_PATH_MAX = 255;
static constexpr size_t LONG_NAME_MAX = 4095;
static constexpr size_t LONG_PATH_MAX = 4096;

struct PathchkOptions {
    bool portability = false;
    bool long_names = false;
    bool suppress_warnings = false;
    size_t name_max = POSIX_NAME_MAX;
    size_t path_max = POSIX_PATH_MAX;
};

static bool check_path(const std::string& path, const PathchkOptions* opts) {
    bool ok = true;

    if (path.empty()) {
        if (!opts->suppress_warnings) {
            fprintf(stderr, "pathchk: '': empty file name\n");
            ok = false;
        }
        return ok;
    }

    if (path.length() > opts->path_max) {
        fprintf(stderr, "pathchk: '%s': File name too long (limit %zu)\n",
                path.c_str(), opts->path_max);
        return false;
    }

    size_t pos = 0;
    bool prev_was_slash = false;
    if (path[0] == '/') {
        pos = 1;
        prev_was_slash = true;
    }

    while (pos <= path.size()) {
        size_t next = path.find('/', pos);
        if (next == std::string::npos) next = path.size();
        std::string comp = path.substr(pos, next - pos);

        if (comp.empty()) {
            if (prev_was_slash && next < path.size()) {
                fprintf(stderr, "pathchk: '%s': empty path component (consecutive slashes)\n",
                        path.c_str());
                return false;
            }
        } else {
            if (comp.length() > opts->name_max) {
                fprintf(stderr, "pathchk: '%s': limit %zu exceeded by length %zu of file name component '%s'\n",
                        path.c_str(), opts->name_max, comp.length(), comp.c_str());
                return false;
            }
            if (comp[0] == '-' && !opts->suppress_warnings) {
                fprintf(stderr, "pathchk: '%s': Warning: file name component '%s' starts with '-'\n",
                        path.c_str(), comp.c_str());
                ok = false;
            }
        }

        prev_was_slash = true;
        if (next >= path.size()) break;
        pos = next + 1;
    }

    if (!opts->portability) {
        struct stat st;
        if (lstat(path.c_str(), &st) == -1) {
            fprintf(stderr, "pathchk: '%s': %s\n", path.c_str(), strerror(errno));
            return false;
        }
        if (access(path.c_str(), R_OK) == -1 && errno == EACCES) {
            fprintf(stderr, "pathchk: '%s': %s\n", path.c_str(), strerror(errno));
            return false;
        }
    }

    return ok;
}

void pathchk_command(int argc, char** argv) {
    PathchkOptions opts;
    bool name_max_set = false;
    size_t custom_name_max = 0;
    std::vector<std::string> files;
    bool end_of_options = false;

    int i = 1;
    while (i < argc) {
        const char* a = argv[i];
        if (end_of_options || a[0] != '-' || strcmp(a, "-") == 0) {
            files.push_back(a);
            i++; continue;
        }
        if (strcmp(a, "--") == 0) { end_of_options = true; i++; continue; }
        if (strcmp(a, "-p") == 0 || strcmp(a, "--portability") == 0) {
            opts.portability = true; i++; continue;
        }
        if (strcmp(a, "-L") == 0 || strcmp(a, "--length") == 0) {
            opts.long_names = true; i++; continue;
        }
        if (strcmp(a, "-w") == 0 || strcmp(a, "--no-check-warnings") == 0) {
            opts.suppress_warnings = true; i++; continue;
        }
        if (strcmp(a, "-n") == 0 || strcmp(a, "--name-max") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "pathchk: option '%s' requires an argument\n", a);
                exit(2);
            }
            char* endp = NULL;
            long val = strtol(argv[i + 1], &endp, 10);
            if (endp == argv[i + 1] || *endp != '\0' || val <= 0) {
                fprintf(stderr, "pathchk: invalid name length: '%s'\n", argv[i + 1]);
                exit(2);
            }
            custom_name_max = (size_t)val;
            name_max_set = true;
            i += 2; continue;
        }
        if (strncmp(a, "--name-max=", 11) == 0) {
            const char* v = a + 11;
            char* endp = NULL;
            long val = strtol(v, &endp, 10);
            if (endp == v || *endp != '\0' || val <= 0) {
                fprintf(stderr, "pathchk: invalid name length: '%s'\n", v);
                exit(2);
            }
            custom_name_max = (size_t)val;
            name_max_set = true;
            i++; continue;
        }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf("Usage: pathchk [OPTION]... FILE...\n");
            printf("Check whether file names are valid or portable.\n");
            printf("\n");
            printf("  -p, --portability       check for POSIX.1 portability; skip existence checks\n");
            printf("  -L, --length            allow names up to %zu characters\n", LONG_NAME_MAX);
            printf("  -n, --name-max=MAX      assume file name components have at most MAX characters\n");
            printf("  -w, --no-check-warnings do not warn about potentially problematic file names\n");
            printf("  -h, --help              display this help and exit\n");
            printf("  -V, --version           output version information and exit\n");
            return;
        }
        if (strcmp(a, "-V") == 0 || strcmp(a, "--version") == 0) {
            printf("pathchk (modbox) 1.0\n");
            printf("Copyright (C) 2026 modbox\n");
            printf("License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>\n");
            return;
        }
        fprintf(stderr, "pathchk: invalid option -- '%s'\nTry 'pathchk --help' for more information.\n", a);
        exit(2);
    }

    if (opts.long_names) {
        opts.name_max = LONG_NAME_MAX;
        opts.path_max = LONG_PATH_MAX;
    }
    if (name_max_set) {
        opts.name_max = custom_name_max;
    }

    if (files.empty()) {
        fprintf(stderr, "pathchk: missing operand\nTry 'pathchk --help' for more information.\n");
        exit(2);
    }

    bool all_ok = true;
    for (const auto& f : files) {
        if (!check_path(f, &opts)) all_ok = false;
    }

    if (!all_ok) exit(1);
}

REGISTER_COMMAND("pathchk", pathchk_command, "Check file names for validity and portability");
