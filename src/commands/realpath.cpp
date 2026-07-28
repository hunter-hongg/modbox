#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include "commands/realpath.hpp"
#include "commands/command_macros.hpp"

extern char** environ;

static std::string resolve_realpath(const char* path, int& error_code, bool& has_error) {
    char* resolved = realpath(path, nullptr);
    if (resolved) {
        std::string result(resolved);
        free(resolved);
        return result;
    }
    error_code = errno;
    has_error = true;
    return "";
}

static std::string resolve_canonical_missing(const char* path, int& error_code, bool& has_error) {
    std::string result(path);
    std::vector<std::pair<ino_t, dev_t>> visited;
    int level = 0;
    const int max_level = 50;

    while (level++ < max_level) {
        struct stat st;
        if (lstat(result.c_str(), &st) == -1) {
            if (errno != ENOENT) {
                error_code = errno;
                has_error = true;
                return "";
            }
            break;
        }

        if (!S_ISLNK(st.st_mode)) {
            break;
        }

        if (st.st_ino != 0 && st.st_dev != 0) {
            for (const auto& v : visited) {
                if (v.first == st.st_ino && v.second == st.st_dev) {
                    error_code = ELOOP;
                    has_error = true;
                    return "";
                }
            }
            visited.emplace_back(st.st_ino, st.st_dev);
        }

        char buffer[PATH_MAX];
        ssize_t n = readlink(result.c_str(), buffer, sizeof(buffer) - 1);
        if (n < 0) {
            error_code = errno;
            has_error = true;
            return "";
        }
        buffer[n] = '\0';

        std::string target(buffer);
        if (target.empty()) {
            error_code = EINVAL;
            has_error = true;
            return "";
        }

        if (target[0] == '/') {
            result = target;
        } else {
            size_t last_slash = result.rfind('/');
            std::string dir = (last_slash == std::string::npos) ? "." : result.substr(0, last_slash + 1);
            result = dir + target;
        }
    }

    return result;
}

static std::string normalize_path(const std::string& path) {
    std::string result;
    bool leading = (path[0] == '/');
    if (leading) result += '/';

    size_t pos = leading ? 1 : 0;
    while (pos < path.size()) {
        size_t next = path.find_first_of('/', pos);
        if (next == std::string::npos) next = path.size();
        std::string comp = path.substr(pos, next - pos);
        pos = next + 1;

        if (comp.empty() || comp == ".") continue;
        if (comp == "..") {
            size_t last = result.rfind('/');
            if (last == std::string::npos) {
                result = "/";
            } else if (last == 0) {
                result = "/";
            } else {
                result.resize(last);
            }
            continue;
        }
        result += comp + '/';
    }
    if (result.length() > 1) result.pop_back();
    return result;
}

static std::string make_relative(const std::string& path, const std::string& relative_to) {
    std::string p = normalize_path(path);
    std::string r = normalize_path(relative_to);

    if (p.empty() || r.empty()) return p;

    std::vector<std::string> p_parts, r_parts;
    size_t pos = 0;
    while (pos < p.size()) {
        size_t next = p.find('/', pos);
        if (next == std::string::npos) next = p.size();
        if (next > pos) p_parts.push_back(p.substr(pos, next - pos));
        pos = next + 1;
    }
    pos = 0;
    while (pos < r.size()) {
        size_t next = r.find('/', pos);
        if (next == std::string::npos) next = r.size();
        if (next > pos) r_parts.push_back(r.substr(pos, next - pos));
        pos = next + 1;
    }

    size_t common = 0;
    while (common < p_parts.size() && common < r_parts.size() && p_parts[common] == r_parts[common]) {
        common++;
    }

    std::string result;
    for (size_t i = common; i < r_parts.size(); i++) {
        result += "../";
    }
    for (size_t i = common; i < p_parts.size(); i++) {
        if (i > common) result += "/";
        result += p_parts[i];
    }
    if (result.empty()) result = ".";
    return result;
}

void realpath_command(int argc, char** argv) {
    bool canonicalize_existing = false;
    bool canonicalize_missing = false;
    bool quiet = false;
    bool no_symlinks = false;
    bool zero = false;
    const char* relative_to = nullptr;
    const char* relative_base = nullptr;
    bool help = false;

    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "-e") == 0 || strcmp(a, "--canonicalize-existing") == 0) {
            canonicalize_existing = true;
        } else if (strcmp(a, "-m") == 0 || strcmp(a, "--canonicalize-missing") == 0) {
            canonicalize_missing = true;
        } else if (strcmp(a, "-L") == 0) {
            canonicalize_existing = true;
        } else if (strcmp(a, "-q") == 0 || strcmp(a, "--quiet") == 0) {
            quiet = true;
        } else if (strcmp(a, "-s") == 0 || strcmp(a, "--strip") == 0 ||
                   strcmp(a, "--no-symlinks") == 0) {
            no_symlinks = true;
        } else if (strcmp(a, "-z") == 0 || strcmp(a, "--zero") == 0) {
            zero = true;
        } else if (strncmp(a, "--relative-to=", 14) == 0) {
            relative_to = a + 14;
        } else if (strncmp(a, "--relative-base=", 16) == 0) {
            relative_base = a + 16;
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            help = true;
        } else {
            files.push_back(a);
        }
    }

    if (help) {
        printf("Usage: %s [OPTION]... FILE...\n", argv[0]);
        printf("Print the resolved absolute pathname.\n");
        printf("\n");
        printf("  -e, --canonicalize-existing  all components must exist\n");
        printf("  -m, --canonicalize-missing   no requirements on components\n");
        printf("  -L                           canonicalize by following symlinks (default)\n");
        printf("  -q, --quiet                  suppress error messages\n");
        printf("  -s, --strip, --no-symlinks   don't resolve symlinks\n");
        printf("  -z, --zero                   end each output line with NUL, not newline\n");
        printf("      --relative-to=FILE       print relative to FILE\n");
        printf("      --relative-base=FILE     print only relative paths under this base\n");
        printf("  -h, --help                   display this help and exit\n");
        return;
    }

    if (files.empty()) {
        if (!quiet) {
            fprintf(stderr, "realpath: missing operand\nTry '%s --help' for more information.\n", argv[0]);
        }
        return;
    }

    int exit_status = 0;

    for (const auto& file : files) {
        std::string resolved;

        if (no_symlinks && !canonicalize_missing) {
            resolved = normalize_path(file);
            if (resolved.empty()) resolved = file;
        } else if (canonicalize_missing) {
            int err = 0;
            bool err_flag = false;
            resolved = resolve_canonical_missing(file.c_str(), err, err_flag);
            if (err_flag && !quiet) {
                fprintf(stderr, "realpath: %s: %s\n", file.c_str(), strerror(err));
                exit_status = 1;
                continue;
            }
        } else {
            int err = 0;
            bool err_flag = false;
            resolved = resolve_realpath(file.c_str(), err, err_flag);
            if (err_flag) {
                if (!quiet) {
                    fprintf(stderr, "realpath: %s: %s\n", file.c_str(), strerror(err));
                }
                exit_status = 1;
                continue;
            }
        }

        std::string output = resolved;

        if (relative_to) {
            output = make_relative(resolved, relative_to);
        }

        if (relative_base) {
            std::string base = normalize_path(relative_base);
            std::string normalized_resolved = normalize_path(resolved);
            if (normalized_resolved.compare(0, base.length(), base) != 0 ||
                (base.length() < normalized_resolved.length() && normalized_resolved[base.length()] != '/')) {
                continue;
            }
            if (relative_to) {
                output = make_relative(resolved, relative_base);
            }
        }

        if (zero) {
            printf("%s%c", output.c_str(), '\0');
        } else {
            printf("%s\n", output.c_str());
        }
    }

    if (exit_status) exit(exit_status);
}
REGISTER_COMMAND("realpath", realpath_command, "Print the resolved absolute pathname");