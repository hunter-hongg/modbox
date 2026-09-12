#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include "commands/which.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

struct WhichOptions {
    bool all = false;         // -a/--all: print every match
    bool skip_dot = false;    // --skip-dot: ignore PATH entries starting with '.'
    bool skip_tilde = false;  // --skip-tilde: ignore PATH entries starting with '~'
    bool show_dot = false;    // --show-dot: keep a '.' PATH entry in output
    bool show_tilde = false;  // --show-tilde: render HOME prefix as '~'
};

void print_help(const char* prog) {
    printf("Usage: %s [options] [--] COMMAND [...]\n", prog);
    printf("Write the full path of COMMAND(s) to standard output.\n");
    printf("\n");
    printf("  -a, --all        Print all matches in PATH, not just the first\n");
    printf("      --skip-dot   Skip directories in PATH that start with a dot\n");
    printf("      --skip-tilde Skip directories in PATH that start with a tilde\n");
    printf("      --show-dot   Don't expand a dot to current directory in output\n");
    printf("      --show-tilde Output a tilde for HOME directory for non-root\n");
    printf("  -h, --help       Display this help and exit\n");
    printf("  -V, --version    Output version information and exit\n");
}

bool is_executable_file(const std::string& path) {
    return access(path.c_str(), X_OK) == 0;
}

// Split a colon-separated PATH string, skipping empty segments.
std::vector<std::string> split_path(const char* path_env) {
    std::vector<std::string> dirs;
    std::string current;
    for (const char* p = path_env; ; ++p) {
        if (*p == ':' || *p == '\0') {
            if (!current.empty()) {
                dirs.push_back(current);
            }
            current.clear();
            if (*p == '\0') {
                break;
            }
        } else {
            current.push_back(*p);
        }
    }
    return dirs;
}

std::string join_dirs(const std::vector<std::string>& dirs) {
    std::string out;
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (i != 0) {
            out.push_back(':');
        }
        out += dirs[i];
    }
    return out;
}

// Apply the dot/tilde display and filtering rules to one PATH entry.
// Returns false when the entry should be skipped entirely.
bool prepare_dir(const std::string& raw, const WhichOptions& opts,
                 std::string* search_dir, std::string* display_prefix) {
    const bool dot = (!raw.empty() && raw[0] == '.');
    const bool tilde = (!raw.empty() && raw[0] == '~');

    if (dot && opts.skip_dot) {
        return false;
    }
    if (tilde && opts.skip_tilde) {
        return false;
    }

    // A relative PATH entry (including '.', '.hidden') is resolved against the
    // current working directory so the printed path is absolute. A bare '.'
    // entry searches the cwd itself unless --show-dot keeps it literal.
    if (!raw.empty() && raw[0] != '/') {
        if (raw == "." && opts.show_dot) {
            *search_dir = raw;
            *display_prefix = raw;
            return true;
        }
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            *search_dir = raw;
            *display_prefix = raw;
            return true;
        }
        if (raw == ".") {
            *search_dir = cwd;
            *display_prefix = cwd;
        } else {
            *search_dir = std::string(cwd) + "/" + raw;
            *display_prefix = *search_dir;
        }
        return true;
    }

    *search_dir = raw;
    *display_prefix = raw;

    // --show-tilde: render a PATH entry that is (or lies under) $HOME using a
    // leading '~', regardless of whether the entry itself starts with '~'.
    if (opts.show_tilde && geteuid() != 0) {
        const char* home = getenv("HOME");
        if (home != nullptr && *home != '\0') {
            std::string const h(home);
            if (raw == h) {
                *display_prefix = "~";
            } else if (raw.size() > h.size() && raw.starts_with(h)
                       && raw[h.size()] == '/') {
                *display_prefix = "~" + raw.substr(h.size());
            }
        }
    }
    return true;
}

// Resolve one name. Returns true if any match was printed.
bool resolve_name(const char* name, const WhichOptions& opts,
                  const std::vector<std::string>& path_dirs) {
    const std::string name_str(name);

    // A name containing a slash is used as-is rather than searched in PATH.
    if (name_str.find('/') != std::string::npos) {
        if (is_executable_file(name_str)) {
            printf("%s\n", name_str.c_str());
            return true;
        }
        (void)fprintf(stderr, "which: no %s in (%s)\n", name,
                      join_dirs(path_dirs).c_str());
        return false;
    }

    bool found = false;
    for (const std::string& raw : path_dirs) {
        std::string search_dir;
        std::string display_prefix;
        if (!prepare_dir(raw, opts, &search_dir, &display_prefix)) {
            continue;
        }
        std::string candidate = search_dir;
        if (!candidate.empty() && candidate.back() != '/') {
            candidate.push_back('/');
        }
        candidate += name_str;
        if (is_executable_file(candidate)) {
            found = true;
            printf("%s/%s\n", display_prefix.c_str(), name);
            if (!opts.all) {
                break;
            }
        }
    }

    if (!found) {
        (void)fprintf(stderr, "which: no %s in (%s)\n", name,
                      join_dirs(path_dirs).c_str());
    }
    return found;
}

}  // namespace

int which_command(int argc, char** argv) {
    const char* prog = argv[0];
    WhichOptions opts;

    int i = 1;
    bool end_of_opts = false;
    for (; i < argc; ++i) {
        const char* a = argv[i];
        if (end_of_opts || a[0] != '-' || a[1] == '\0') {
            break;  // first non-option operand (or "--" consumed below)
        }
        if (strcmp(a, "--") == 0) {
            ++i;
            end_of_opts = true;
            break;
        }
        if (strcmp(a, "-a") == 0 || strcmp(a, "--all") == 0) {
            opts.all = true;
        } else if (strcmp(a, "--skip-dot") == 0) {
            opts.skip_dot = true;
        } else if (strcmp(a, "--skip-tilde") == 0) {
            opts.skip_tilde = true;
        } else if (strcmp(a, "--show-dot") == 0) {
            opts.show_dot = true;
        } else if (strcmp(a, "--show-tilde") == 0) {
            opts.show_tilde = true;
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_help(prog);
            return 0;
        } else if (strcmp(a, "-V") == 0 || strcmp(a, "--version") == 0) {
            print_version("which");
            return 0;
        } else {
            // Unknown option: report the offending character and exit 2.
            (void)fprintf(stderr, "%s: invalid option -- '%c'\n", prog,
                          a[1] == '-' ? a[2] : a[1]);
            return 2;
        }
    }

    const char* path_env = getenv("PATH");
    if (path_env == nullptr || *path_env == '\0') {
        path_env = "/usr/bin:/bin";
    }
    std::vector<std::string> const path_dirs = split_path(path_env);

    if (i >= argc) {
        // No command names: print usage to stderr and exit 0, matching GNU
        // which with no arguments.
        print_help(prog);
        return 0;
    }

    int matched = 0;
    int total = 0;
    for (int k = i; k < argc; ++k) {
        ++total;
        if (resolve_name(argv[k], opts, path_dirs)) {
            ++matched;
        }
    }

    if (matched == total) {
        return 0;
    }
    if (matched == 0) {
        return 2;
    }
    return 1;
}

REGISTER_COMMAND("which", which_command, "Locate a command in PATH");
