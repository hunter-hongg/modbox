#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <selinux/selinux.h>
#include <cstdint>
#include <cstring>
#include <clocale>
#include <cerrno>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/tree.hpp"
#include "commands/version_util.hpp"

namespace {

struct Node {
    std::string name;      // base name, or the path as given for a walk root
    std::string full;      // path from the walk root
    bool is_dir = false;   // sorts/counts as a directory (symlink to dir included)
    bool recurse = false;  // descend into it (needs -l for symlinks)
    bool is_symlink = false;
    bool stat_ok = false;     // lstat succeeded, so there is metadata to show
    bool open_error = false;  // opendir() failed on this directory
    bool recursive_link = false;  // -l target was already expanded elsewhere
    uint64_t size = 0;        // st_size
    uint64_t du = 0;          // st_size of this entry plus, for dirs, all descendants
    uint64_t inode = 0;
    time_t mtime = 0;
    time_t ctime = 0;
    mode_t mode = 0;          // lstat mode, used by -F
    mode_t target_mode = 0;   // stat mode of a symlink target, used by -F
    dev_t dev = 0;
    ino_t ino = 0;
    dev_t content_dev = 0;    // directory identity for the -l cycle guard
    ino_t content_ino = 0;
    std::string context;
    std::string link_target;
    std::vector<Node> children;
};

// -I/-P patterns: GNU tree splits the specification on '|' and matches each
// piece as a shell glob against the whole entry name.
struct Filter {
    std::vector<std::string> exclude;
    std::vector<std::string> include;
    bool has_exclude = false;
    bool has_include = false;
};

struct WalkCtx {
    const TreeOptions* opts;
    const Filter* filter;
    dev_t root_dev = 0;
    int dirs = 0;
    int files = 0;
    int errors = 0;
    uint64_t total_du = 0;
    // Cycle guard for -l: (dev, ino) of directories already fully expanded
    // as symlink targets.
    std::set<std::pair<dev_t, ino_t>> visited;
};

// An empty piece is in the list; a bare "" option is not.
std::vector<std::string> split_patterns(const std::string& spec) {
    std::vector<std::string> out;
    if (spec.empty()) return out;
    size_t pos = 0;
    while (true) {
        size_t next = spec.find('|', pos);
        if (next == std::string::npos) {
            out.push_back(spec.substr(pos));
            break;
        }
        out.push_back(spec.substr(pos, next - pos));
        pos = next + 1;
    }
    return out;
}

// A list holding an empty piece matches every name, as in GNU tree. A malformed
// glob matches nothing.
bool matches_glob(const std::vector<std::string>& patterns, const std::string& name) {
    for (const std::string& p : patterns) {
        if (p.empty() || fnmatch(p.c_str(), name.c_str(), 0) == 0) return true;
    }
    return false;
}

// GNU tree shows a name the way ls does: printable characters as they are,
// anything else as a three digit octal escape per byte.
std::string octal_escape(unsigned char c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "\\%03o", c);
    return buf;
}

std::string escape_name(const std::string& s) {
    std::string out;
    mbstate_t state{};
    size_t pos = 0;
    while (pos < s.size()) {
        wchar_t wc = 0;
        size_t len = mbrtowc(&wc, s.c_str() + pos, s.size() - pos, &state);
        if (len == static_cast<size_t>(-1)) {
            std::memset(&state, 0, sizeof(state));
            out += octal_escape(static_cast<unsigned char>(s[pos]));
            pos += 1;
            continue;
        }
        if (len == static_cast<size_t>(-2) || len == 0) {
            // Truncated multibyte sequence: escape the bytes it starts with.
            for (size_t i = pos; i < s.size(); i++) {
                out += octal_escape(static_cast<unsigned char>(s[i]));
            }
            break;
        }
        if (iswprint(wc) != 0) out.append(s, pos, len);
        else {
            for (size_t i = 0; i < len; i++) {
                out += octal_escape(static_cast<unsigned char>(s[pos + i]));
            }
        }
        pos += len;
    }
    return out;
}

// GNU tree sorts entries with the C library collation of the current locale.
int coll_compare(const Node& a, const Node& b) {
    int rc = strcoll(a.name.c_str(), b.name.c_str());
    if (rc != 0) return rc;
    return std::strcmp(a.name.c_str(), b.name.c_str());
}

bool name_less(const Node& a, const Node& b) {
    return coll_compare(a, b) < 0;
}

uint64_t size_value(const Node& n, const TreeOptions& opts) {
    return opts.show_du ? n.du : n.size;
}

// GNU tree carries only an integer count of the previous unit into the next
// division, so 1206072 bytes is 1177 KiB truncated, then 1.1M, not 1.2M.
std::string human_size(uint64_t bytes) {
    static constexpr char suffixes[] = "KMGTPE";
    constexpr size_t max_steps = sizeof(suffixes) - 1;
    char buf[32];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(bytes));
        return buf;
    }
    uint64_t carry = bytes;
    size_t steps = 0;
    double v;
    while (steps < max_steps) {
        uint64_t count = carry / 1024;
        if (count < 1024 || steps + 1 == max_steps) {
            v = static_cast<double>(carry) / 1024.0;
            steps++;
            break;
        }
        carry = count;
        steps++;
    }
    // A value that rounds up to 10.0 loses its decimal, so 10239 is "10K".
    if (v < 9.95) {
        std::snprintf(buf, sizeof(buf), "%.1f%c", v, suffixes[steps - 1]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f%c", v, suffixes[steps - 1]);
    }
    return buf;
}

std::string right_justify(const std::string& s, int width) {
    if (static_cast<int>(s.size()) >= width) return s;
    return std::string(static_cast<size_t>(width - s.size()), ' ') + s;
}

std::string date_field(const Node& n, const TreeOptions& opts) {
    // GNU tree reports the status change time once -c selects it for sorting.
    time_t t = opts.sort_mode == 4 ? n.ctime : n.mtime;
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[64];
    time_t now;
    time(&now);
    // Six months (of 31 days) old, or dated in the future, and the clock gives
    // way to the year, as in ls long listings.
    if (t > now || difftime(now, t) >= 186 * 86400.0) {
        std::strftime(buf, sizeof(buf), "%b %e  %Y", &tmv);
    } else {
        std::strftime(buf, sizeof(buf), "%b %e %H:%M", &tmv);
    }
    return buf;
}

bool show_size(const TreeOptions& opts) {
    return opts.show_size != 0 || opts.show_du != 0;
}

bool excluded(const Node& n, const WalkCtx& ctx) {
    return ctx.filter->has_exclude && matches_glob(ctx.filter->exclude, n.name);
}

// GNU tree's -P only ever filters files; directories stay so their matching
// descendants remain reachable. The test is on the entry itself, so a symlink
// to a directory is filtered like any other file.
bool included(const Node& n, const WalkCtx& ctx) {
    if (!ctx.filter->has_include) return true;
    if (S_ISDIR(n.mode)) return true;
    return matches_glob(ctx.filter->include, n.name);
}

// Recursively fold each entry's st_size into its parent directory.
void aggregate(Node& node) {
    node.du = node.size;
    if (!node.is_dir) return;
    for (Node& child : node.children) {
        aggregate(child);
        node.du += child.du;
    }
}

// Load metadata (size, inode, dates, security context) for an existing node.
void fill_stat(Node& node, WalkCtx& ctx, const struct stat& st) {
    node.stat_ok = true;
    node.dev = st.st_dev;
    node.ino = st.st_ino;
    node.inode = st.st_ino;
    node.mtime = st.st_mtime;
    node.ctime = st.st_ctime;
    node.size = static_cast<uint64_t>(st.st_size);
    node.mode = st.st_mode;
    node.is_symlink = S_ISLNK(st.st_mode);
    node.is_dir = S_ISDIR(st.st_mode);
    node.content_dev = st.st_dev;
    node.content_ino = st.st_ino;
    node.recurse = node.is_dir;
    if (ctx.opts->show_selinux && is_selinux_enabled() > 0) {
        char* con = nullptr;
        if (lgetfilecon(node.full.c_str(), &con) >= 0 && con != nullptr) {
            node.context = con;
            freecon(con);
        }
    }
    if (node.is_symlink) {
        char buf[4096];
        ssize_t n = readlink(node.full.c_str(), buf, sizeof(buf) - 1);
        if (n >= 0) {
            buf[n] = '\0';
            node.link_target = buf;
        }
        // GNU tree sorts, groups and counts a symlink to a directory as a
        // directory, but only descends into it when -l asks for it.
        struct stat target;
        if (stat(node.full.c_str(), &target) == 0) {
            node.target_mode = target.st_mode;
            if (S_ISDIR(target.st_mode)) {
                node.is_dir = true;
                node.content_dev = target.st_dev;
                node.content_ino = target.st_ino;
                if (ctx.opts->follow_links) node.recurse = true;
            }
        }
    }
}

// Three-way comparison on the selected key, with entry names breaking ties.
// GNU tree sizes largest-first; the other keys sort ascending.
int key_compare(const Node& a, const Node& b, const TreeOptions* opts) {
    switch (opts->sort_mode) {
        // Version sort ignores locale collation, unlike every other mode.
        case 1: return strverscmp(a.name.c_str(), b.name.c_str());
        case 2: {
            uint64_t sa = size_value(a, *opts), sb = size_value(b, *opts);
            if (sa != sb) return sa > sb ? -1 : 1;
            break;
        }
        case 3:
            if (a.mtime != b.mtime) return a.mtime < b.mtime ? -1 : 1;
            break;
        case 4:
            if (a.ctime != b.ctime) return a.ctime < b.ctime ? -1 : 1;
            break;
        default: break;
    }
    return coll_compare(a, b);
}

bool name_before(const Node& a, const Node& b, const TreeOptions* opts) {
    if (opts->dirs_first && a.is_dir != b.is_dir) return a.is_dir;
    if (opts->files_first && a.is_dir != b.is_dir) return b.is_dir;
    if (opts->sort_mode == 5) return false;  // none: keep directory order
    int cmp = key_compare(a, b, opts);
    if (opts->reverse) cmp = -cmp;
    return cmp < 0;
}

// GNU tree opens a directory only when its children are displayable, so at
// -L level the entries on that level are listed but never read.
bool depth_openable(int depth, const TreeOptions* opts) {
    return opts->max_depth == 0 || depth < opts->max_depth;
}

void read_dir(Node& node, WalkCtx& ctx, int depth);

// GNU tree expands each directory once; a symlink reaching one that is already
// explored is listed but not followed. A real directory is always expanded,
// even when a symlink reached it first.
void descend(Node& child, WalkCtx& ctx, int depth) {
    if (!child.recurse) return;
    auto key = std::make_pair(child.content_dev, child.content_ino);
    if (child.is_symlink && ctx.visited.count(key) > 0) {
        child.recursive_link = true;
        return;
    }
    ctx.visited.insert(key);
    read_dir(child, ctx, depth + 1);
}

void read_dir(Node& node, WalkCtx& ctx, int depth) {
    DIR* d = opendir(node.full.c_str());
    if (d == nullptr) {
        node.open_error = true;
        // GNU tree only fails the run on directories it reached by walking;
        // a directory it cannot read under --du is reported but not counted.
        if (depth > 0 && !ctx.opts->show_du) ctx.errors++;
        return;
    }
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
        if (!ctx.opts->show_hidden && ent->d_name[0] == '.') continue;
        Node child;
        child.name = ent->d_name;
        // A trailing slash on the argument must not double up in the path -f
        // prints.
        child.full = (node.full.back() == '/' ? node.full : node.full + "/") + child.name;
        struct stat st;
        if (lstat(child.full.c_str(), &st) != 0) continue;  // vanished while walking
        fill_stat(child, ctx, st);
        if (excluded(child, ctx)) continue;
        if (ctx.opts->one_filesystem && child.is_dir && child.dev != ctx.root_dev) continue;
        node.children.push_back(std::move(child));
        // GNU tree totals --du while it still reads the directory, so the
        // already-seen decision there is taken in readdir order; without --du
        // it descends after sorting, i.e. in display order.
        if (ctx.opts->show_du && depth_openable(depth + 1, ctx.opts)) {
            descend(node.children.back(), ctx, depth);
        }
    }
    closedir(d);
    // --sort=none must keep readdir order: sorting with an all-equivalent
    // comparator would still shuffle the entries.
    if (ctx.opts->sort_mode != 5) {
        std::sort(node.children.begin(), node.children.end(),
                  [&ctx](const Node& a, const Node& b) { return name_before(a, b, ctx.opts); });
    }
    if (!ctx.opts->show_du && depth_openable(depth + 1, ctx.opts)) {
        for (Node& child : node.children) descend(child, ctx, depth);
    }
}

// Drop non-matching entries and tally the report counters. The walk root is
// handled by the caller: GNU tree counts it only when it shows children.
void prune_and_count(Node& node, WalkCtx& ctx) {
    std::vector<Node> kept;
    kept.reserve(node.children.size());
    for (Node& child : node.children) {
        if (!included(child, ctx)) continue;
        if (ctx.opts->dirs_only && !child.is_dir) continue;
        prune_and_count(child, ctx);
        kept.push_back(std::move(child));
    }
    node.children = std::move(kept);
    for (Node& child : node.children) {
        if (child.is_dir) ctx.dirs++;
        else ctx.files++;
    }
}

const char* connector_branch(const TreeOptions* opts) {
    return opts->ascii_charset ? "|-- " : "├── ";
}
const char* connector_last(const TreeOptions* opts) {
    return opts->ascii_charset ? "`-- " : "└── ";
}
const char* connector_pipe(const TreeOptions* opts) {
    return opts->ascii_charset ? "|   " : "│   ";
}

// GNU tree groups every numeric field into one bracket: inode, size, date.
std::string metadata_field(const Node& n, const TreeOptions& opts) {
    std::string out;
    if (opts.show_selinux && !n.context.empty()) {
        out += "[";
        out += n.context;
        out += "]  ";
    }
    std::string inner;
    if (opts.show_inodes) {
        inner += right_justify(std::to_string(n.inode), 7);
    }
    if (show_size(opts)) {
        if (!inner.empty()) inner += " ";
        uint64_t v = opts.show_du ? n.du : n.size;
        inner += opts.human_size ? right_justify(human_size(v), 4)
                                 : right_justify(std::to_string(v), 11);
    }
    if (opts.show_date) {
        if (!inner.empty()) inner += " ";
        inner += date_field(n, opts);
    }
    if (!inner.empty()) {
        out += "[";
        out += inner;
        out += "]  ";
    }
    return out;
}

// -F indicator, judged from what the entry resolves to.
char classify_suffix(const Node& n) {
    mode_t m = n.is_symlink ? n.target_mode : n.mode;
    if (m == 0) return '\0';
    if (S_ISDIR(m)) return '/';
    if (S_ISSOCK(m)) return '=';
    if (S_ISFIFO(m)) return '|';
    if ((m & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) return '*';
    return '\0';
}

void print_entry_line(const Node& n, const std::string& prefix, bool is_last, bool root,
                      const TreeOptions& opts) {
    if (!root && !opts.no_indent) {
        std::printf("%s%s", prefix.c_str(),
                    is_last ? connector_last(&opts) : connector_branch(&opts));
    }
    // GNU tree suppresses -F indicators when only directories are listed.
    // A symbolic-link argument keeps the '@' marker and no target: tree only
    // reads link targets while scanning directories.
    char indicator = '\0';
    if (opts.classify && !opts.dirs_only) {
        indicator = root && n.is_symlink ? '@' : classify_suffix(n);
    }
    std::string line;
    if (n.stat_ok) line += metadata_field(n, opts);
    line += escape_name(opts.full_path && !root ? n.full : n.name);
    const bool show_target = !root && !n.link_target.empty();
    if (!show_target) {
        if (indicator != '\0') line += indicator;
    } else {
        // For a symlink the indicator qualifies the target, so it follows it.
        line += " -> ";
        line += escape_name(n.link_target);
        if (indicator != '\0') line += indicator;
    }
    if (n.recursive_link) line += "  [recursive, not followed]";
    if (n.open_error) line += "  [error opening dir]";
    std::printf("%s\n", line.c_str());
}

void print_tree(const Node& node, const std::string& prefix, bool is_last,
                const TreeOptions& opts, bool root) {
    print_entry_line(node, prefix, is_last, root, opts);
    if (node.open_error || node.children.empty()) return;
    std::string child_prefix;
    if (!root && !opts.no_indent) {
        child_prefix = prefix + (is_last ? "    " : connector_pipe(&opts));
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        print_tree(node.children[i], child_prefix, i + 1 == node.children.size(), opts, false);
    }
}

void print_report(const WalkCtx& ctx, const TreeOptions& opts) {
    if (opts.noreport) return;
    std::printf("\n");
    if (opts.show_du) {
        std::string used = opts.human_size ? right_justify(human_size(ctx.total_du), 5)
                                           : right_justify(std::to_string(ctx.total_du), 12);
        std::printf("%s %sin %d %s", used.c_str(),
                    opts.human_size ? "used " : "bytes used ", ctx.dirs,
                    ctx.dirs == 1 ? "directory" : "directories");
        if (!opts.dirs_only) {
            std::printf(", %d %s", ctx.files, ctx.files == 1 ? "file" : "files");
        }
        std::printf("\n");
        return;
    }
    std::printf("%d %s", ctx.dirs, ctx.dirs == 1 ? "directory" : "directories");
    if (!opts.dirs_only) {
        std::printf(", %d %s", ctx.files, ctx.files == 1 ? "file" : "files");
    }
    std::printf("\n");
}

}  // namespace

int tree_command(int argc, char** argv) {
    TreeOptions opts{};
    // GNU tree orders entries with the user's locale collation (strcoll).
    std::setlocale(LC_ALL, "");

    struct arg_lit* opt_a = arg_lit0("a", "all", "show hidden files");
    struct arg_lit* opt_d = arg_lit0("d", NULL, "list directories only");
    struct arg_lit* opt_x = arg_lit0("x", NULL, "stay on one filesystem");
    struct arg_lit* opt_l = arg_lit0("l", NULL, "follow symbolic links");
    struct arg_lit* opt_D = arg_lit0("D", NULL, "print modification dates");
    struct arg_lit* opt_F = arg_lit0("F", NULL, "append indicator characters");
    struct arg_lit* opt_s = arg_lit0("s", NULL, "print file sizes in bytes");
    struct arg_lit* opt_du = arg_lit0(NULL, "du", "print cumulative size of directories");
    struct arg_lit* opt_h = arg_lit0("h", NULL, "print sizes in human-readable form (implies -s)");
    struct arg_lit* opt_ino = arg_lit0(NULL, "inodes", "print inode numbers");
    struct arg_lit* opt_selinux = arg_lit0(NULL, "selinux", "print SELinux security context");
    struct arg_lit* opt_f = arg_lit0("f", NULL, "print full path prefix");
    struct arg_lit* opt_norep = arg_lit0(NULL, "noreport", "omit summary line");
    struct arg_lit* opt_i = arg_lit0("i", NULL, "do not indent lines");
    struct arg_lit* opt_dirsfirst = arg_lit0(NULL, "dirsfirst", "list directories before files");
    struct arg_lit* opt_filesfirst = arg_lit0(NULL, "filesfirst", "list files before directories");
    struct arg_lit* opt_r = arg_lit0("r", NULL, "reverse the order of the sort");
    struct arg_lit* opt_v = arg_lit0("v", NULL, "sort by version (same as --sort=version)");
    struct arg_lit* opt_t = arg_lit0("t", NULL, "sort by modification time (same as --sort=mtime)");
    struct arg_lit* opt_c = arg_lit0("c", NULL, "sort by status change time (same as --sort=ctime)");
    struct arg_lit* opt_U = arg_lit0("U", NULL, "do not sort (same as --sort=none)");
    struct arg_str* opt_I = arg_str0("I", NULL, "PATTERN", "do not list names matching glob PATTERN");
    struct arg_str* opt_P = arg_str0("P", NULL, "PATTERN", "list only files matching glob PATTERN");
    struct arg_str* opt_charset = arg_str0(NULL, "charset", "CHARSET", "output charset (ascii)");
    struct arg_str* opt_sort = arg_str0(NULL, "sort", "TYPE", "sort by name|version|size|mtime|ctime|none");
    struct arg_int* opt_L = arg_int0("L", NULL, "LEVEL", "max display depth");
    struct arg_lit* opt_help = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* opt_version = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_str* paths = arg_strn(NULL, NULL, "path", 0, argc, "paths");
    struct arg_end* end = arg_end(20);

    ArgTable at({opt_a, opt_d, opt_x, opt_l, opt_D, opt_F, opt_s, opt_du, opt_h,
                 opt_ino, opt_selinux, opt_f, opt_norep, opt_i, opt_dirsfirst, opt_filesfirst,
                 opt_r, opt_v, opt_t, opt_c, opt_U,
                 opt_I, opt_P, opt_charset, opt_sort, opt_L, opt_help, opt_version,
                 paths, end});

    int const nerrors = at.parse(argc, argv);

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (opt_help->count > 0) {
        std::printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        std::printf("List contents of directories in a tree-like format.\n");
        std::printf("\nOptions:\n");
        std::printf("  -a, --all            all files are listed\n");
        std::printf("  -d                   list directories only\n");
        std::printf("  -l                   follow symbolic links like directories\n");
        std::printf("  -f                   print the full path prefix for each file\n");
        std::printf("  -x                   stay on current filesystem only\n");
        std::printf("  -L LEVEL             descend only LEVEL directories deep\n");
        std::printf("  -P PATTERN           list only files that match glob PATTERN\n");
        std::printf("  -I PATTERN           do not list files that match glob PATTERN\n");
        std::printf("  --charset CHARSET    charset for the line drawing (ascii)\n");
        std::printf("  -s                   print the size in bytes of each file\n");
        std::printf("  -h                   print sizes in human readable form (implies -s)\n");
        std::printf("  --du                 compute size of directories by their contents\n");
        std::printf("  --inodes             print inode number of each file\n");
        std::printf("  -D                   print the last modification date of each file\n");
        std::printf("  -F                   append /, =, | or * to each file as per ls -F\n");
        std::printf("  --selinux            print the SELinux security context of each file\n");
        std::printf("  --sort TYPE          select sort: name|version|size|mtime|ctime|none\n");
        std::printf("  -v                   sort files alphanumerically by version\n");
        std::printf("  -t                   sort files by last modification time\n");
        std::printf("  -c                   sort files by last status change time\n");
        std::printf("  -U                   leave files unsorted\n");
        std::printf("  -r                   reverse the order of the sort\n");
        std::printf("  --dirsfirst          list directories before files\n");
        std::printf("  --filesfirst         list files before directories\n");
        std::printf("  -i                   do not print indentation lines\n");
        std::printf("  --noreport           turn off file/directory count at end of listing\n");
        std::printf("      --help           display this help and exit\n");
        std::printf("      --version        output version information and exit\n");
        return 0;
    }

    if (opt_version->count > 0) {
        print_version("tree");
        return 0;
    }

    opts.show_hidden = opt_a->count > 0;
    opts.dirs_only = opt_d->count > 0;
    opts.one_filesystem = opt_x->count > 0;
    opts.follow_links = opt_l->count > 0;
    opts.show_date = opt_D->count > 0;
    opts.classify = opt_F->count > 0;
    opts.show_size = opt_s->count > 0;
    opts.show_du = opt_du->count > 0;
    opts.human_size = opt_h->count > 0;
    opts.show_inodes = opt_ino->count > 0;
    opts.show_selinux = opt_selinux->count > 0;
    opts.full_path = opt_f->count > 0;
    opts.noreport = opt_norep->count > 0;
    opts.no_indent = opt_i->count > 0;
    opts.dirs_first = opt_dirsfirst->count > 0;
    opts.files_first = opt_filesfirst->count > 0;
    opts.reverse = opt_r->count > 0;
    if (opt_I->count > 0) opts.exclude_pattern = opt_I->sval[0];
    if (opt_P->count > 0) opts.include_pattern = opt_P->sval[0];
    if (opt_charset->count > 0 && std::strcmp(opt_charset->sval[0], "ascii") == 0) {
        opts.ascii_charset = 1;
    }
    if (opt_L->count > 0) opts.max_depth = opt_L->ival[0];
    if (opt_v->count > 0) opts.sort_mode = 1;
    if (opt_t->count > 0) opts.sort_mode = 3;
    if (opt_c->count > 0) opts.sort_mode = 4;
    if (opt_U->count > 0) opts.sort_mode = 5;
    if (opt_sort->count > 0) {
        const char* m = opt_sort->sval[0];
        if (std::strcmp(m, "version") == 0) opts.sort_mode = 1;
        else if (std::strcmp(m, "size") == 0) opts.sort_mode = 2;
        else if (std::strcmp(m, "mtime") == 0) opts.sort_mode = 3;
        else if (std::strcmp(m, "ctime") == 0) opts.sort_mode = 4;
        else if (std::strcmp(m, "none") == 0) opts.sort_mode = 5;
        else opts.sort_mode = 0;
    }

    for (int i = 0; i < paths->count; i++) opts.paths.push_back(paths->sval[i]);

    // GNU tree rejects -L 0 as well as negative levels.
    if (opts.max_depth < 1 && opt_L->count > 0) {
        std::fprintf(stderr, "%s: Invalid level, must be greater than 0.\n", argv[0]);
        return 1;
    }
    if (opts.human_size && !opts.show_size && !opts.show_du) opts.show_size = 1;
    if (opts.paths.empty()) opts.paths.push_back(".");

    Filter filter;
    filter.has_exclude = opt_I->count > 0;
    filter.has_include = opt_P->count > 0;
    filter.exclude = split_patterns(opts.exclude_pattern);
    filter.include = split_patterns(opts.include_pattern);

    WalkCtx ctx;
    ctx.opts = &opts;
    ctx.filter = &filter;
    int exit_code = 0;

    for (const std::string& root_path : opts.paths) {
        Node root;
        root.name = root_path;
        root.full = root_path;
        if (opts.full_path) {
            // GNU tree shows -f paths without the trailing slash of the argument.
            while (root.full.size() > 1 && root.full.back() == '/') root.full.pop_back();
            root.name = root.full;
        }
        struct stat st;
        if (lstat(root_path.c_str(), &st) != 0) {
            root.open_error = true;
            print_entry_line(root, "", true, true, opts);
            ctx.errors++;
            exit_code = 2;
            continue;
        }
        fill_stat(root, ctx, st);
        if (!root.is_dir) {
            // GNU tree reports a non-directory argument as an unopenable dir.
            root.open_error = true;
            print_entry_line(root, "", true, true, opts);
            ctx.files++;
            continue;
        }
        ctx.root_dev = st.st_dev;
        ctx.visited.insert(std::make_pair(root.content_dev, root.content_ino));
        read_dir(root, ctx, 0);
        prune_and_count(root, ctx);
        aggregate(root);
        ctx.total_du += root.du;

        // GNU tree counts a walk root only when the root itself is listed with
        // at least one child; an empty or unopenable root counts as nothing or
        // as a single file.
        if (root.open_error) ctx.files++;
        else if (!root.children.empty()) ctx.dirs++;

        print_tree(root, "", true, opts, true);
    }

    print_report(ctx, opts);

    if (ctx.errors > 0) exit_code = 2;
    return exit_code;
}

REGISTER_COMMAND("tree", tree_command, "List contents of directories in a tree-like format");
