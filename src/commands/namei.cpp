// modbox namei — follow a pathname and print each component
//
// Behavior modeled on util-linux misc-utils/namei.c (v2.42.3). Each input path
// is split into a list of component nodes; every node caches its own lstat(2)
// result (or the errno that lstat failed with). Symlinks are resolved by
// splicing their target's components after them with level+1 indentation.
// The short output format is:
//
//   f: <path as given>
//    d /
//    d tmp
//     - f.txt
//
// with two leading spaces per indent level and one extra space before the
// type character in short mode; -v moves the indent after the mode, -m prints
// the full rwx string, -o appends owner/group padded to the widest name seen
// in the whole run (the util-linux idcache rule).

#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <grp.h>
#include <list>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/namei.hpp"
#include "commands/version_util.hpp"

namespace {

// Symlink chain limit, matching MAXSYMLINKS in the upstream source.
constexpr int kMaxSymlinks = 256;

// Option flags. Kept as plain bits so `-l` = modes | owners | vertical is
// readable at the call site.
enum : int {
    FlagsModes = 1 << 1,
    FlagsOwners = 1 << 2,
    FlagsMounts = 1 << 3,
    FlagsVertical = 1 << 5,
};

int g_flags = 0;
bool g_follow_links = true;

// One resolved path component, mirroring util-linux' struct namei.
struct Namei {
    std::string name;
    std::string abslink;  // symlink target made absolute, for the `->` display
    int relstart = 0;     // offset inside abslink where the printed target starts
    int level = 0;
    int noent = 0;        // errno from lstat, or 0 when the lstat succeeded
    bool mountpoint = false;
    bool is_symlink = false;
    struct stat st {};
    bool has_stat = false;
};

// util-linux idcache: user/group names with a width of > 0 across the whole run.
struct IdCache {
    struct Entry {
        unsigned long id;
        std::string name;
    };
    std::vector<Entry> entries;
    std::size_t width = 0;

    void add(unsigned long id, const std::string& name) {
        entries.push_back({id, name});
        if (name.size() > width) width = name.size();
    }
    std::string get(unsigned long id) const {
        for (const auto& e : entries)
            if (e.id == id) return e.name;
        return {};
    }
};

IdCache g_ucache;
IdCache g_gcache;

void add_uid(unsigned long id) {
    for (const auto& e : g_ucache.entries)
        if (e.id == id) return;
    std::vector<char> buf(256);
    struct passwd pw;
    struct passwd* res = nullptr;
    errno = 0;
    if (getpwuid_r(static_cast<uid_t>(id), &pw, buf.data(), buf.size(), &res) != 0) res = nullptr;
    std::string name = (res && res->pw_name && res->pw_name[0] != '\0') ? res->pw_name
                                                                         : std::to_string(id);
    g_ucache.add(id, name);
}

void add_gid(unsigned long id) {
    for (const auto& e : g_gcache.entries)
        if (e.id == id) return;
    std::vector<char> buf(256);
    struct group gr;
    struct group* res = nullptr;
    errno = 0;
    if (getgrgid_r(static_cast<gid_t>(id), &gr, buf.data(), buf.size(), &res) != 0) res = nullptr;
    std::string name = (res && res->gr_name && res->gr_name[0] != '\0') ? res->gr_name
                                                                         : std::to_string(id);
    g_gcache.add(id, name);
}

// The 10-character mode string as xstrmode(3) renders it: a leading type
// character then the rwx triples, with s/S/t/T for the special bits.
std::string short_mode(mode_t m) {
    char out[11];
    switch (m & S_IFMT) {
        case S_IFLNK: out[0] = 'l'; break;
        case S_IFSOCK: out[0] = 's'; break;
        case S_IFREG: out[0] = '-'; break;
        case S_IFDIR: out[0] = 'd'; break;
        case S_IFBLK: out[0] = 'b'; break;
        case S_IFCHR: out[0] = 'c'; break;
        case S_IFIFO: out[0] = 'p'; break;
        default: out[0] = '?'; break;
    }
    out[1] = (m & S_IRUSR) ? 'r' : '-';
    out[2] = (m & S_IWUSR) ? 'w' : '-';
    out[3] = (m & S_IXUSR) ? 'x' : '-';
    if (m & S_ISUID) out[3] = (m & S_IXUSR) ? 's' : 'S';
    out[4] = (m & S_IRGRP) ? 'r' : '-';
    out[5] = (m & S_IWGRP) ? 'w' : '-';
    out[6] = (m & S_IXGRP) ? 'x' : '-';
    if (m & S_ISGID) out[6] = (m & S_IXGRP) ? 's' : 'S';
    out[7] = (m & S_IROTH) ? 'r' : '-';
    out[8] = (m & S_IWOTH) ? 'w' : '-';
    out[9] = (m & S_IXOTH) ? 'x' : '-';
    if (m & S_ISVTX) out[9] = (m & S_IXOTH) ? 't' : 'T';
    out[10] = '\0';
    return std::string(out);
}

// readlink_to_namei: build the absolute-ish form of a symlink target. A
// relative target is prefixed by the link's directory; an absolute target (or
// a bare file name with no slash) is used as-is. relstart marks the offset of
// the printed target text inside abslink.
void readlink_to_namei(Namei& n, const std::string& path) {
    std::vector<char> buf(4096);
    ssize_t sz = readlink(path.c_str(), buf.data(), buf.size());
    if (sz <= 0) return;  // unreadable target: leave abslink empty
    std::string target(buf.data(), static_cast<std::size_t>(sz));
    if (!target.empty() && target[0] != '/') {
        std::string::size_type p = path.find_last_of('/');
        if (p == 0) {
            n.abslink = "/" + target;
            n.relstart = 1;
        } else if (p != std::string::npos) {
            n.abslink = path.substr(0, p) + "/" + target;
            n.relstart = static_cast<int>(p) + 1;
        } else {
            n.abslink = target;
            n.relstart = 0;
        }
    } else {
        n.abslink = target;
        n.relstart = 0;
    }
}

// Build one component: lstat its path (recording errno on failure), record the
// symlink target when appropriate, and decide whether it is a mount point.
Namei make_component(const std::string& name, const std::string& path, int level,
                     const Namei* prev) {
    Namei n;
    n.name = name;
    n.level = level;
    if (lstat(path.c_str(), &n.st) != 0) {
        n.noent = errno;
        return n;
    }
    n.has_stat = true;
    if (S_ISLNK(n.st.st_mode)) {
        n.is_symlink = true;
        readlink_to_namei(n, path);
    }
    if (g_flags & FlagsOwners) {
        add_uid(static_cast<unsigned long>(n.st.st_uid));
        add_gid(static_cast<unsigned long>(n.st.st_gid));
    }
    if ((g_flags & FlagsMounts) && S_ISDIR(n.st.st_mode)) {
        const struct stat* sb = nullptr;
        struct stat dotdot {};
        if (prev && S_ISDIR(prev->st.st_mode)) {
            sb = &prev->st;
        } else if (!prev || (prev->has_stat && S_ISLNK(prev->st.st_mode))) {
            std::string dd = path + "/..";
            if (stat(dd.c_str(), &dotdot) == 0) sb = &dotdot;
        }
        if (sb && (sb->st_dev != n.st.st_dev || sb->st_ino == n.st.st_ino)) n.mountpoint = true;
    }
    return n;
}

// Build the component list for `orgpath` starting at offset `start`. All
// components share `level` (the top-level path uses level 0; a symlink's
// target components use the symlink's level + 1). `prev` is the node this
// chain continues from (nullptr for the first component of a path).
std::list<Namei> build_chain(const std::string& orgpath, std::size_t start, int level,
                             const Namei* prev) {
    std::list<Namei> out;
    if (orgpath.empty() || start >= orgpath.size()) return out;

    std::size_t p = start;
    std::string accum;
    // Leading slashes collapse to a single root component.
    if (orgpath[p] == '/') {
        while (p < orgpath.size() && orgpath[p] == '/') p++;
        out.push_back(make_component("/", "/", level, prev));
        prev = &out.back();
        accum = "/";
    } else {
        // A relative symlink target starts past the link's directory; the
        // prefix before `start` is that directory and must stay part of the
        // path we lstat so the target resolves relative to the link.
        accum = orgpath.substr(0, start);
    }
    while (p < orgpath.size()) {
        std::size_t end = orgpath.find('/', p);
        std::size_t len = (end == std::string::npos) ? (orgpath.size() - p) : (end - p);
        std::string comp = orgpath.substr(p, len);
        if (accum.empty())
            accum = comp;
        else if (accum.back() == '/')
            accum += comp;
        else
            accum += "/" + comp;
        out.push_back(make_component(comp, accum, level, prev));
        prev = &out.back();
        if (end == std::string::npos) break;
        p = end + 1;
        while (p < orgpath.size() && orgpath[p] == '/') p++;
    }
    return out;
}

// Resolve every symlink in the chain, splicing the target's components after
// the link. Returns -1 when the symlink limit is exceeded.
int follow_symlinks(std::list<Namei>& chain) {
    int symcount = 0;
    for (auto it = chain.begin(); it != chain.end();) {
        if (it->noent || !it->is_symlink) {
            ++it;
            continue;
        }
        if (++symcount > kMaxSymlinks) return -1;
        auto next_it = std::next(it);
        std::list<Namei> target = build_chain(it->abslink, static_cast<std::size_t>(it->relstart),
                                              it->level + 1, &*it);
        chain.splice(next_it, target);
        ++it;  // advance into the freshly spliced target (follows chains)
    }
    return 0;
}

// Print one component and return -1 if it printed a failure line (missing or
// inaccessible), so the caller can propagate the exit status.
int print_component(const Namei& n) {
    std::string md = short_mode(n.st.st_mode);
    if (n.mountpoint) md[0] = 'D';

    if (n.noent != 0) {
        int blanks = 1;
        if (g_flags & FlagsModes) blanks += 9;
        if (g_flags & FlagsOwners)
            blanks += static_cast<int>(g_ucache.width + g_gcache.width + 2);
        if (!(g_flags & FlagsVertical)) blanks += 1;
        blanks += 1;  // !context (no SELinux context column)
        blanks += n.level * 2;
        std::printf("%*s ", blanks, "");
        std::printf("%s - %s\n", n.name.c_str(), std::strerror(n.noent));
        return -1;
    }

    if (!(g_flags & FlagsVertical)) {
        for (int i = 0; i < n.level; i++) std::fputs("  ", stdout);
        std::fputc(' ', stdout);
    }

    if (g_flags & FlagsModes)
        std::printf("%s", md.c_str());
    else
        std::printf("%c", md[0]);

    if (g_flags & FlagsOwners) {
        std::printf(" %-*s", static_cast<int>(g_ucache.width),
                    g_ucache.get(static_cast<unsigned long>(n.st.st_uid)).c_str());
        std::printf(" %-*s", static_cast<int>(g_gcache.width),
                    g_gcache.get(static_cast<unsigned long>(n.st.st_gid)).c_str());
    }

    if (g_flags & FlagsVertical)
        for (int i = 0; i < n.level; i++) std::fputs("  ", stdout);

    if (n.is_symlink)
        std::printf(" %s -> %s\n", n.name.c_str(),
                    n.abslink.substr(static_cast<std::size_t>(n.relstart)).c_str());
    else
        std::printf(" %s\n", n.name.c_str());
    return 0;
}

int print_namei(const std::list<Namei>& chain, const std::string& path) {
    std::printf("f: %s\n", path.c_str());
    for (const auto& n : chain) {
        if (print_component(n) != 0) return -1;
    }
    return 0;
}

void print_help(const char* prog) {
    std::printf("Usage: %s [options] <pathname>...\n", prog);
    std::printf("Follow a pathname until a terminal point is found.\n");
    std::printf("\n");
    std::printf(" -x, --mountpoints   show mount point directories with a 'D'\n");
    std::printf(" -m, --modes         show the mode bits of each file\n");
    std::printf(" -o, --owners        show owner and group name of each file\n");
    std::printf(" -l, --long          use a long listing format (-m -o -v)\n");
    std::printf(" -n, --nosymlinks    don't follow symlinks\n");
    std::printf(" -v, --vertical      vertical align of modes and owners\n");
    std::printf(" -h, --help          display this help and exit\n");
    std::printf(" -V, --version       output version information and exit\n");
}

}  // namespace

int namei_command(int argc, char** argv) {
    struct arg_lit* opt_mounts = arg_lit0("x", "mountpoints", "show mount point directories with a 'D'");
    struct arg_lit* opt_modes = arg_lit0("m", "modes", "show the mode bits of each file");
    struct arg_lit* opt_owners = arg_lit0("o", "owners", "show owner and group name of each file");
    struct arg_lit* opt_long = arg_lit0("l", "long", "use a long listing format (-m -o -v)");
    struct arg_lit* opt_nolinks = arg_lit0("n", "nosymlinks", "don't follow symlinks");
    struct arg_lit* opt_vertical = arg_lit0("v", "vertical", "vertical align of modes and owners");
    struct arg_lit* opt_help = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* opt_version = arg_lit0("V", "version", "output version information and exit");
    struct arg_file* paths = arg_filen(nullptr, nullptr, "<pathname>", 0, 1000, "pathname to follow");
    struct arg_end* end = arg_end(20);

    ArgTable table({opt_mounts, opt_modes, opt_owners, opt_long, opt_nolinks, opt_vertical,
                    opt_help, opt_version, paths, end});

    int const nerrors = table.parse(argc, argv);
    if (nerrors != 0) {
        (void)print_arg_errors(end, "namei");
        return 2;
    }

    if (opt_help->count > 0) {
        print_help("namei");
        return 0;
    }
    if (opt_version->count > 0) {
        print_version("namei");
        return 0;
    }

    g_flags = 0;
    if (opt_modes->count > 0) g_flags |= FlagsModes;
    if (opt_owners->count > 0) g_flags |= FlagsOwners;
    if (opt_mounts->count > 0) g_flags |= FlagsMounts;
    if (opt_vertical->count > 0) g_flags |= FlagsVertical;
    if (opt_long->count > 0) g_flags |= (FlagsModes | FlagsOwners | FlagsVertical);
    g_follow_links = opt_nolinks->count == 0;

    if (paths->count == 0) {
        std::fprintf(stderr, "namei: missing operand\n");
        std::fprintf(stderr, "Try 'namei --help' for more information.\n");
        return 2;
    }

    // First pass: do whole-path stat on all paths and build all chains.
    // This allows computing global owner/group widths before printing.
    struct PathInfo {
        std::string path;
        bool whole_stat_ok = false;
        int whole_stat_errno = 0;
        struct stat whole_st {};
        std::list<Namei> chain;
        int symlink_limit = 0;
    };
    std::vector<PathInfo> all_paths;
    all_paths.reserve(paths->count);

    int rc = 0;
    for (int i = 0; i < paths->count; i++) {
        std::string path = paths->filename[i];
        PathInfo pi;
        pi.path = path;
        if (path.empty()) {
            rc = 1;
            all_paths.push_back(std::move(pi));
            continue;
        }
        // Whole-path stat (follows symlinks) - must succeed for the path to be valid.
        if (stat(path.c_str(), &pi.whole_st) == 0) {
            pi.whole_stat_ok = true;
        } else {
            pi.whole_stat_errno = errno;
        }
        pi.chain = build_chain(path, 0, 0, nullptr);
        if (g_follow_links) pi.symlink_limit = follow_symlinks(pi.chain);
        all_paths.push_back(std::move(pi));
    }

    // Second pass: print all chains with global widths.
    for (auto& pi : all_paths) {
        // If whole-path stat failed, check if any component already has an error.
        // If no component error, mark the last component with the whole-path errno.
        if (!pi.whole_stat_ok) {
            bool any_component_error = false;
            for (const auto& n : pi.chain) {
                if (n.noent != 0) {
                    any_component_error = true;
                    break;
                }
            }
            if (!any_component_error && !pi.chain.empty()) {
                // Mark the last component with the whole-path errno (e.g., ENOTDIR for
                // trailing slash on a file).
                auto& last = const_cast<Namei&>(pi.chain.back());
                last.noent = pi.whole_stat_errno;
                last.has_stat = false;
            }
            rc = 1;
        }
        if (print_namei(pi.chain, pi.path) != 0) rc = 1;
        if (pi.symlink_limit == -1) {
            std::fprintf(stderr, "namei: %s: exceeded limit of symlinks\n", pi.path.c_str());
            rc = 1;
        }
    }
    return rc;
}

REGISTER_COMMAND("namei", namei_command, "Follow a pathname until a terminal point is found");
