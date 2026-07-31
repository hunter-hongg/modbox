#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits.h>

#include "commands/readlink.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static constexpr size_t READLINK_BUFFER = 4096;

std::string resolve_canonical(const char* path, int& error_code, bool& has_error) {
    std::string result(path);
    std::vector<std::pair<ino_t, dev_t>> visited;
    int level = 0;
    const int max_level = 50;

    while (level++ < max_level) {
        struct stat st;
        if (lstat(result.c_str(), &st) == -1) {
            error_code = errno;
            has_error = true;
            return "";
        }

        if (!S_ISLNK(st.st_mode)) {
            std::string normalized;
            bool leading = (result[0] == '/');
            if (leading) normalized += '/';

            size_t pos = leading ? 1 : 0;
            while (pos < result.size()) {
                size_t next = result.find_first_of('/', pos);
                if (next == std::string::npos) next = result.size();
                std::string comp = result.substr(pos, next - pos);
                pos = next + 1;

                if (comp.empty() || comp == ".") continue;
                if (comp == "..") {
                    size_t last = normalized.rfind('/');
                    if (last == std::string::npos) {
                        normalized = "/";
                    } else if (last == 0) {
                        normalized = "/";
                    } else {
                        normalized.resize(last);
                    }
                    continue;
                }
                normalized += comp + '/';
            }
            if (normalized.length() > 1) normalized.pop_back();
            result = normalized;
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

        char buffer[READLINK_BUFFER];
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

    if (level >= max_level) {
        error_code = ELOOP;
        has_error = true;
        return "";
    }

    struct stat final_st;
    if (lstat(result.c_str(), &final_st) == -1) {
        error_code = ENOENT;
        has_error = true;
        return "";
    }

    return result;
}

static void strip_trailing_whitespace(std::string& s) {
    size_t i = s.size();
    while (i > 0) {
        char c = s[i-1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            i--;
        } else {
            break;
        }
    }
    s = s.substr(0, i);
}

static bool maybe_strip_parens(std::string& s) {
    if (s.size() >= 2 && s.front() == '(' && s.back() == ')') {
        s = s.substr(1, s.size() - 2);
        return true;
    }
    return false;
}

int readlink_command(int argc, char** argv) {
    bool canonicalize = false;
    bool quiet = false;
    bool strip = false;
    bool no_newline = false;
    int help = 0, version = 0;

    // Simple option parsing: scan all arguments first
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const char* opt = argv[i];
        if (strcmp(opt, "-f") == 0 || strcmp(opt, "--canonicalize") == 0) {
            canonicalize = true;
            continue;
        }
        if (strcmp(opt, "-q") == 0 || strcmp(opt, "--no-error") == 0) {
            quiet = true;
            continue;
        }
        if (strcmp(opt, "-s") == 0 || strcmp(opt, "--strip") == 0) {
            strip = true;
            continue;
        }
        if (strcmp(opt, "-n") == 0 || strcmp(opt, "--no-dereference") == 0) {
            no_newline = true;
            continue;
        }
        if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
            help = 1;
            continue;
        }
        if (strcmp(opt, "-V") == 0 || strcmp(opt, "--version") == 0) {
            version = 1;
            continue;
        }
        // Non-option argument is a file
        files.push_back(opt);
    }

    if (help) {
        printf("Usage: readlink [OPTION]... FILE\n");
        printf("Write the contents of SYMBOLIC LINK to standard output.\n");
        printf("\n");
        printf("  -f, --canonicalize          resolve all symbolic links\n");
        printf("  -q, --no-error              ignore nonexistent invalid inputs\n");
        printf("  -s, --strip                 strip trailing whitespace\n");
        printf("  -n, --no-dereference        don't add newline\n");
        printf("  -h, --help                  display this help and exit\n");
        return 0;
    }

    if (version) {
        print_version("readlink");
        printf("Copyright (C) 2026 modbox\n");
        printf("License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>\n");
        return 0;
    }

    if (files.empty()) {
        // Try to read from stdin
        char buffer[READLINK_BUFFER];
        if (fgets(buffer, sizeof(buffer), stdin)) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
            if ((int)strlen(buffer) > 0) files.push_back(buffer);
        }
    }

    if (files.empty()) {
        if (!quiet) {
            fprintf(stderr, "readlink: missing operand\nTry '%s --help' for more information.\n", argv[0]);
        }
        return 0;
    }

    for (const auto& file : files) {
        struct stat st;
        if (lstat(file.c_str(), &st) == -1) {
            if (!quiet) {
                fprintf(stderr, "readlink: %s: %s\n", file.c_str(), strerror(errno));
            }
            continue;
        }

        if (!S_ISLNK(st.st_mode)) {
            if (!quiet) {
                fprintf(stderr, "readlink: %s: not a symbolic link\n", file.c_str());
            }
            continue;
        }

        char buffer[READLINK_BUFFER];
        ssize_t n = readlink(file.c_str(), buffer, sizeof(buffer) - 1);
        if (n < 0) {
            if (!quiet) {
                fprintf(stderr, "readlink: %s: %s\n", file.c_str(), strerror(errno));
            }
            continue;
        }
        buffer[n] = '\0';

        std::string output(buffer);

        if (canonicalize) {
            int err = 0;
            bool err_flag = false;
            output = resolve_canonical(output.c_str(), err, err_flag);
            if (err_flag) {
                if (!quiet) {
                    if (err == ELOOP) {
                        fprintf(stderr, "readlink: %s: Too many levels of symbolic links\n", file.c_str());
                    } else {
                        fprintf(stderr, "readlink: %s: %s\n", file.c_str(), strerror(err));
                    }
                }
            }
        }

        if (strip) {
            maybe_strip_parens(output);
            strip_trailing_whitespace(output);
        }

        if (no_newline && canonicalize) {
            printf("%s", output.c_str());
        } else {
            printf("%s\n", output.c_str());
        }
    }
    return 0;
}

REGISTER_COMMAND("readlink", readlink_command, "Print target of a symbolic link");
