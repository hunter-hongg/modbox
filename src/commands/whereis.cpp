#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/whereis.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

struct WhereisOptions {
    bool bin = false;  // -b
    bool man = false;  // -m
    bool src = false;  // -s
    bool unusual = false;  // -u
};

void print_help(const char* prog) {
    printf("Usage: %s [options] [-BMS <dir>... -f] <name>\n", prog);
    printf("Locate the binary, source, and manual-page files for a command.\n");
    printf("\n");
    printf(" -b         search only for binaries\n");
    printf(" -B <dirs>  define binaries lookup path\n");
    printf(" -m         search only for manuals and infos\n");
    printf(" -M <dirs>  define man and info lookup path\n");
    printf(" -s         search only for sources\n");
    printf(" -S <dirs>  define sources lookup path\n");
    printf(" -f         terminate <dirs> argument list\n");
    printf(" -u         search for unusual entries\n");
    printf(" -l         output effective lookup paths\n");
    printf("\n");
    printf(" -h, --help     display this help\n");
    printf(" -V, --version  display version\n");
}

// ---------------------------------------------------------------------------
// Directory-tree helpers
// ---------------------------------------------------------------------------

bool is_executable_file(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    return access(path.c_str(), X_OK) == 0;
}

bool is_regular_file(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

bool is_dir(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

std::string join(const std::string& dir, const std::string& name) {
    if (dir.empty()) {
        return name;
    }
    if (dir.back() == '/') {
        return dir + name;
    }
    return dir + "/" + name;
}

// ---------------------------------------------------------------------------
// PATH / default roots
// ---------------------------------------------------------------------------

std::vector<std::string> split_colon(const char* path_env) {
    std::vector<std::string> entries;
    std::string current;
    for (const char* p = path_env; ; ++p) {
        if (*p == ':' || *p == '\0') {
            if (!current.empty()) {
                entries.push_back(current);
            }
            current.clear();
            if (*p == '\0') {
                break;
            }
        } else {
            current.push_back(*p);
        }
    }
    return entries;
}

std::string real_path(const std::string& path) {
    char resolved[4096];
    if (realpath(path.c_str(), resolved) != nullptr) {
        return resolved;
    }
    return path;
}

// Keep only the first entry for each distinct resolved directory. This mirrors
// util-linux whereis, which lists /usr/bin once even though /bin and /sbin are
// symlinks to it.
std::vector<std::string> dedup_by_real(const std::vector<std::string>& dirs) {
    std::vector<std::string> out;
    std::vector<std::string> seen;
    for (const std::string& d : dirs) {
        std::string const real = real_path(d);
        bool dup = false;
        for (const std::string& s : seen) {
            if (s == real) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            seen.push_back(real);
            out.push_back(d);
        }
    }
    return out;
}

std::vector<std::string> default_bin_roots() {
    std::vector<std::string> roots = {
        "/usr/local/bin", "/usr/local/sbin", "/usr/bin",
        "/usr/sbin",      "/bin",           "/sbin",
    };
    const char* path_env = getenv("PATH");
    if (path_env != nullptr) {
        for (const std::string& dir : split_colon(path_env)) {
            roots.push_back(dir);
        }
    }
    return dedup_by_real(roots);
}

std::vector<std::string> default_man_roots() {
    std::vector<std::string> roots;
    const char* bases[] = {"/usr/local/man", "/usr/share/man", "/usr/man"};
    for (const char* base : bases) {
        if (!is_dir(base)) {
            continue;
        }
        for (int section = 1; section <= 9; ++section) {
            std::string const sub = join(base, "man" + std::to_string(section));
            if (is_dir(sub)) {
                roots.push_back(sub);
            }
        }
    }
    const char* info_dirs[] = {"/usr/local/info", "/usr/share/info"};
    for (const char* d : info_dirs) {
        if (is_dir(d)) {
            roots.push_back(d);
        }
    }
    return roots;
}

std::vector<std::string> default_src_roots() {
    std::vector<std::string> roots;
    const char* bases[] = {"/usr/local/src", "/usr/share/src", "/usr/src"};
    for (const char* d : bases) {
        if (is_dir(d)) {
            roots.push_back(d);
        }
    }
    return roots;
}

// ---------------------------------------------------------------------------
// Per-class search
// ---------------------------------------------------------------------------

// Binaries: an executable regular file named exactly <name>.
std::vector<std::string> search_bin(const std::string& name,
                                    const std::vector<std::string>& roots) {
    std::vector<std::string> hits;
    for (const std::string& root : roots) {
        std::string const candidate = join(root, name);
        if (is_executable_file(candidate)) {
            hits.push_back(candidate);
        }
    }
    return hits;
}

// Sources: a regular file <name>.c (the modbox subset).
std::vector<std::string> search_src(const std::string& name,
                                    const std::vector<std::string>& roots) {
    std::vector<std::string> hits;
    for (const std::string& root : roots) {
        std::string const candidate = join(root, name + ".c");
        if (is_regular_file(candidate)) {
            hits.push_back(candidate);
        }
    }
    return hits;
}

// Manuals: <name>.<section> with an optional compression suffix, plus info
// files <name>.info with an optional compression suffix.
std::vector<std::string> search_man(const std::string& name,
                                    const std::vector<std::string>& roots) {
    static const char* const kCompress[] = {".gz", ".xz", ".bz2", ".lzma", ""};
    std::vector<std::string> hits;
    for (const std::string& root : roots) {
        for (int section = 1; section <= 9; ++section) {
            for (const char* comp : kCompress) {
                std::string const candidate =
                    join(root, name + "." + std::to_string(section) + comp);
                if (is_regular_file(candidate)) {
                    hits.push_back(candidate);
                }
            }
        }
        for (const char* comp : kCompress) {
            std::string const candidate = join(root, name + ".info" + comp);
            if (is_regular_file(candidate)) {
                hits.push_back(candidate);
            }
        }
    }
    return hits;
}

void print_roots(const char* label, const std::vector<std::string>& roots) {
    for (const std::string& r : roots) {
        printf("%s: %s\n", label, r.c_str());
    }
}

// Consume the whitespace-separated directory list that follows -B/-M/-S, up to
// the next option (or the -f terminator).
void consume_dir_list(int argc, char** argv, int* i,
                      std::vector<std::string>* dest) {
    ++(*i);
    while (*i < argc) {
        const char* d = argv[*i];
        if (d[0] == '-' || strcmp(d, "-f") == 0) {
            break;
        }
        dest->push_back(d);
        ++(*i);
    }
}

enum class ParseResult { Ok, Done, Error };

struct WantedClasses {
    bool bin = true;
    bool man = true;
    bool src = true;
};

WantedClasses wanted_classes(const WhereisOptions& opts) {
    if (!opts.bin && !opts.man && !opts.src) {
        return WantedClasses{};  // no -b/-m/-s → all three
    }
    return WantedClasses{opts.bin, opts.man, opts.src};
}

void print_hits(const std::vector<std::string>& hits) {
    for (const std::string& p : hits) {
        printf(" %s", p.c_str());
    }
}

// util-linux reports the final path component of the requested name, so
// `whereis /usr/bin/ls` prints `ls:` and searches for `ls`.
std::string name_label(const std::string& name) {
    const size_t slash = name.rfind('/');
    if (slash == std::string::npos) {
        return name;
    }
    return name.substr(slash + 1);
}

// Report one name. In -u mode, a name is unusual unless every requested class
// has exactly one hit; complete names are suppressed.
void report_name(const std::string& name, const WhereisOptions& opts,
                 const WantedClasses& want,
                 const std::vector<std::string>& b,
                 const std::vector<std::string>& m,
                 const std::vector<std::string>& s) {
    if (opts.unusual) {
        const bool complete =
            (!want.bin || b.size() == 1) && (!want.man || m.size() == 1)
            && (!want.src || s.size() == 1);
        if (complete) {
            return;
        }
    }

    printf("%s:", name_label(name).c_str());
    if (want.bin) {
        print_hits(b);
    }
    if (want.man) {
        print_hits(m);
    }
    if (want.src) {
        print_hits(s);
    }
    printf("\n");
}

// Parse the option prefix. On Ok, *i points at the first name (or argc when
// there are none). On Done, help/version was printed and the caller should
// return 0. On Error the caller should return 2.
ParseResult parse_options(int argc, char** argv, const char* prog,
                          WhereisOptions* opts, int* i, bool* print_paths,
                          std::vector<std::string>* bin_roots,
                          std::vector<std::string>* man_roots,
                          std::vector<std::string>* src_roots,
                          bool* have_bin, bool* have_man, bool* have_src) {
    *i = 1;
    while (*i < argc) {
        const char* a = argv[*i];
        if (a[0] != '-' || a[1] == '\0') {
            break;  // first name; option parsing is done
        }
        if (strcmp(a, "--") == 0) {
            ++(*i);
            break;
        }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_help(prog);
            return ParseResult::Done;
        }
        if (strcmp(a, "-V") == 0 || strcmp(a, "--version") == 0) {
            print_version("whereis");
            return ParseResult::Done;
        }
        if (strcmp(a, "-b") == 0) {
            opts->bin = true;
        } else if (strcmp(a, "-m") == 0) {
            opts->man = true;
        } else if (strcmp(a, "-s") == 0) {
            opts->src = true;
        } else if (strcmp(a, "-u") == 0) {
            opts->unusual = true;
        } else if (strcmp(a, "-l") == 0) {
            *print_paths = true;
        } else if (strcmp(a, "-f") == 0) {
            // Terminates the directory list; nothing to do here.
        } else if (strcmp(a, "-B") == 0) {
            *have_bin = true;
            consume_dir_list(argc, argv, i, bin_roots);
            continue;
        } else if (strcmp(a, "-M") == 0) {
            *have_man = true;
            consume_dir_list(argc, argv, i, man_roots);
            continue;
        } else if (strcmp(a, "-S") == 0) {
            *have_src = true;
            consume_dir_list(argc, argv, i, src_roots);
            continue;
        } else {
            (void)fprintf(stderr, "%s: unrecognized option '%s'\n", prog, a);
            (void)fprintf(stderr, "Try '%s --help' for more information.\n",
                          prog);
            return ParseResult::Error;
        }
        ++(*i);
    }
    return ParseResult::Ok;
}

}  // namespace

int whereis_command(int argc, char** argv) {
    const char* prog = argv[0];
    WhereisOptions opts;

    std::vector<std::string> bin_roots;
    std::vector<std::string> man_roots;
    std::vector<std::string> src_roots;
    bool have_bin = false;
    bool have_man = false;
    bool have_src = false;

    bool print_paths = false;

    int i = 1;
    const ParseResult parse = parse_options(argc, argv, prog, &opts, &i,
                                            &print_paths, &bin_roots, &man_roots,
                                            &src_roots, &have_bin, &have_man,
                                            &have_src);
    if (parse == ParseResult::Done) {
        return 0;
    }
    if (parse == ParseResult::Error) {
        return 2;
    }

    std::vector<std::string> names;
    for (; i < argc; ++i) {
        names.push_back(argv[i]);
    }

    // Resolve the effective search roots (class defaults are independent).
    if (!have_bin) {
        bin_roots = default_bin_roots();
    }
    if (!have_man) {
        man_roots = default_man_roots();
    }
    if (!have_src) {
        src_roots = default_src_roots();
    }

    if (print_paths) {
        print_roots("bin", bin_roots);
        print_roots("man", man_roots);
        print_roots("src", src_roots);
        return 0;
    }

    // Which classes to report: all three unless one or more -b/-m/-s is given.
    const WantedClasses want = wanted_classes(opts);

    for (const std::string& name : names) {
        std::string const base = name_label(name);
        std::vector<std::string> const b =
            want.bin ? search_bin(base, bin_roots) : std::vector<std::string>();
        std::vector<std::string> const m =
            want.man ? search_man(base, man_roots) : std::vector<std::string>();
        std::vector<std::string> const s =
            want.src ? search_src(base, src_roots) : std::vector<std::string>();

        report_name(name, opts, want, b, m, s);
    }

    return 0;
}

REGISTER_COMMAND("whereis", whereis_command,
                 "Locate binary, source, and manual files");
