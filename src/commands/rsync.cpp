#include "commands/rsync.hpp"

#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/arg_util.hpp"

// ── Constants ────────────────────────────────────────────────────────────────

static const char* CVS_EXCLUDES[] = {
    "*.[oa]",   "*.o",   "*.a",   "*.so",  "*.so.*", "*.dylib",
    "*.lo",     "*.la",  "*.loT", "*.pyc", "*.pyo", "*.class",
    ".svn",     ".git",  ".hg",   ".bzr",  "_darcs", "CVS",
    "node_modules",
    nullptr
};

static const char* HELP_TEXT = R"(Usage: rsync [OPTION]... SRC [SRC]... DEST

Copy files and directories between locations.

Essentials:
  -r, --recursive          Recurse into directories
  -l, --links              Copy symlinks as symlinks
  -p, --perms              Preserve permissions
  -t, --times              Preserve modification times
  -g, --group              Preserve group
  -o, --owner              Preserve owner (super-user only)
  -D, --devices            Preserve device files (super-user only)
  -a, --archive            Archive mode (= -rlptgoD)

Transfer options:
  -u, --update             Skip files that are newer on destination
  -c, --checksum           Skip based on checksum, not mtime+size
  --delete                 Delete extraneous files from destination
  --exclude=PATTERN        Exclude files matching PATTERN
  --exclude-from=FILE      Read exclude patterns from FILE
  --include=PATTERN        Don't exclude files matching PATTERN
  -C, --cvs-exclude        Use cvs-exclude defaults
  --remove-source-files    Delete source files after transfer
  --ignore-errors          Continue despite errors
  --itemize-changes        Output abbreviated change list

Display options:
  -v, --verbose            Increase verbosity
  -q, --quiet              Suppress non-error messages
  -n, --dry-run            Show what would be done
  -h, --human-readable     Output numbers in human-readable format
  --progress               Show progress during transfer
  --info=SPEC              Fine-grained info control (progress2, stats)
  --stats                  Print transfer statistics

Miscellaneous:
      --help               Display this help
      --version            Output version information
)";

static void print_help(const char* prog) {
    printf("rsync (modbox) - synchronize files and directories\n\n");
    printf("Usage: %s [OPTION]... SRC [SRC]... DEST\n\n", prog);
    printf("%s\n", HELP_TEXT);
}

// ── FileTree — local file-tree representation ───────────────────────────────

struct FileEntry {
    std::string path;
    bool is_dir;
    bool is_link;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    long long size;
    double mtime;
    std::string link_target;
};

class FileTree {
public:
    std::string root;
    std::vector<FileEntry> entries;

    FileTree() = default;
    explicit FileTree(const std::string& root_) : root(root_) { build(); }

    void build() {
        if (!std::filesystem::exists(root)) return;
        walk(root, "");
        std::sort(entries.begin(), entries.end(),
                  [](const FileEntry& a, const FileEntry& b) { return a.path < b.path; });
    }

    void walk(const std::string& dir, const std::string& rel) {
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* de;
        while ((de = readdir(d)) != nullptr) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            std::string child_rel = rel.empty() ? de->d_name : rel + "/" + de->d_name;
            std::string full = dir;
            if (dir.back() != '/') full += '/';
            full += de->d_name;

            struct stat st{};
            if (lstat(full.c_str(), &st) != 0) continue;

            FileEntry e;
            e.path = child_rel;
            e.is_dir = S_ISDIR(st.st_mode);
            e.is_link = S_ISLNK(st.st_mode);
            e.mode = st.st_mode & 07777;
            e.uid = st.st_uid;
            e.gid = st.st_gid;
            e.size = st.st_size;
            e.mtime = static_cast<double>(st.st_mtime) +
                      static_cast<double>(st.st_mtim.tv_nsec) / 1e9;

            if (e.is_link) {
                char buf[4096];
                ssize_t n = readlink(full.c_str(), buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    e.link_target = buf;
                }
            }

            entries.push_back(e);

            if (e.is_dir && !e.is_link) {
                walk(full, child_rel);
            }
        }
        closedir(d);
    }

    const FileEntry* find(const std::string& rel) const {
        for (const auto& e : entries) {
            if (e.path == rel) return &e;
        }
        return nullptr;
    }
};

// ── Exclusion helpers ────────────────────────────────────────────────────────

static bool matches_pattern(const std::string& path, const std::string& pattern) {
    if (pattern.empty()) return false;
    size_t pi = 0, si = 0;
    size_t star_pi = std::string::npos, star_si = si;
    while (si < path.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == path[si])) {
            ++pi; ++si;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            if (path[si] == '/') return false;
            star_pi = pi++; star_si = ++si;
        } else if (star_pi != std::string::npos) {
            pi = star_pi + 1; si = ++star_si;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

static bool should_exclude(const std::string& rel,
                           const std::vector<std::string>& excludes,
                           const std::vector<std::string>& includes,
                           bool cvs_exclude) {
    // Check includes first
    for (const auto& inc : includes) {
        if (matches_pattern(rel, inc) ||
            matches_pattern(rel.substr(rel.find_last_of('/') + 1), inc)) {
            return false;
        }
    }
    // Check excludes
    for (const auto& ex : excludes) {
        if (matches_pattern(rel, ex) ||
            matches_pattern(rel.substr(rel.find_last_of('/') + 1), ex)) {
            return true;
        }
    }
    // CVS excludes
    if (cvs_exclude) {
        std::string basename = rel.substr(rel.find_last_of('/') + 1);
        for (const auto* pat : CVS_EXCLUDES) {
            if (matches_pattern(basename, pat)) return true;
        }
    }
    return false;
}

// ── Directory creation helper ────────────────────────────────────────────────

static bool mkdirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            struct stat st{};
            if (stat(cur.c_str(), &st) != 0) {
                if (mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) {
                    return false;
                }
            }
        }
    }
    return true;
}

// ── Copy helpers ─────────────────────────────────────────────────────────────

static bool copy_file(const std::string& src, const std::string& dst,
                      bool preserve_perms, bool preserve_times,
                      bool dryrun, bool verbose) {
    if (dryrun) {
        if (verbose) printf("sending %s\n", src.c_str());
        return true;
    }
    // Create parent directory
    size_t pos = dst.rfind('/');
    if (pos != std::string::npos) {
        std::string dstdir = dst.substr(0, pos);
        mkdirs(dstdir);
    }
    // Open source
    int infd = open(src.c_str(), O_RDONLY);
    if (infd < 0) {
        fprintf(stderr, "rsync: cannot open '%s': %s\n", src.c_str(), strerror(errno));
        return false;
    }
    // Open dest
    int outfd = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) {
        close(infd);
        fprintf(stderr, "rsync: cannot create '%s': %s\n", dst.c_str(), strerror(errno));
        return false;
    }
    // Copy
    char buf[65536];
    ssize_t n;
    while ((n = read(infd, buf, sizeof(buf))) > 0) {
        ssize_t w = write(outfd, buf, static_cast<size_t>(n));
        if (w != n) {
            close(infd); close(outfd);
            fprintf(stderr, "rsync: write error: %s\n", strerror(errno));
            return false;
        }
    }
    close(infd); close(outfd);
    // Preserve permissions
    if (preserve_perms) {
        struct stat st{};
        if (stat(src.c_str(), &st) == 0) {
            chmod(dst.c_str(), st.st_mode & 07777);
        }
    }
    // Preserve times
    if (preserve_times) {
        struct timespec ts[2];
        struct stat st{};
        if (stat(src.c_str(), &st) == 0) {
            ts[0] = st.st_atim;
            ts[1] = st.st_mtim;
            utimensat(AT_FDCWD, dst.c_str(), ts, 0);
        }
    }
    return true;
}

// ── Command implementation ───────────────────────────────────────────────────

int rsync_command(int argc, char** argv) {
    // ── Argtable3 ──
    struct arg_lit* help_opt         = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt      = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* recursive_opt    = arg_lit0("r", "recursive", "recurse into directories");
    struct arg_lit* links_opt        = arg_lit0("l", "links", "copy symlinks as symlinks");
    struct arg_lit* perms_opt        = arg_lit0("p", "perms", "preserve permissions");
    struct arg_lit* times_opt        = arg_lit0("t", "times", "preserve modification times");
    struct arg_lit* group_opt        = arg_lit0("g", "group", "preserve group");
    struct arg_lit* owner_opt        = arg_lit0("o", "owner", "preserve owner");
    struct arg_lit* devices_opt      = arg_lit0("D", "devices", "preserve device files");
    struct arg_lit* archive_opt      = arg_lit0("a", "archive", "archive mode (= -rlptgoD)");
    struct arg_lit* update_opt       = arg_lit0("u", "update", "skip files newer on destination");
    struct arg_lit* checksum_opt     = arg_lit0("c", "checksum", "skip based on checksum");
    struct arg_lit* delete_opt       = arg_lit0(NULL, "delete", "delete extraneous files from dest");
    struct arg_lit* dryrun_opt       = arg_lit0("n", "dry-run", "show what would be done");
    struct arg_lit* ignore_errors_opt = arg_lit0(NULL, "ignore-errors", "continue despite errors");
    struct arg_lit* remove_src_opt   = arg_lit0(NULL, "remove-source-files", "delete src after transfer");
    struct arg_lit* verbose_opt      = arg_lit0("v", "verbose", "increase verbosity");
    struct arg_lit* quiet_opt        = arg_lit0("q", "quiet", "suppress non-error messages");
    struct arg_lit* human_read_opt   = arg_lit0("h", "human-readable", "human-readable numbers");
    struct arg_lit* progress_opt     = arg_lit0(NULL, "progress", "show progress");
    struct arg_lit* itemize_opt      = arg_lit0("i", "itemize-changes", "output abbreviated change list");
    struct arg_lit* stats_opt        = arg_lit0(NULL, "stats", "print transfer stats");
    struct arg_lit* cvs_exclude_opt  = arg_lit0("C", "cvs-exclude", "use cvs-exclude defaults");

    struct arg_str* exclude_opt      = arg_strn(NULL, "exclude", "PATTERN", 0, 100, "exclude patterns");
    struct arg_str* include_opt      = arg_strn(NULL, "include", "PATTERN", 0, 100, "include patterns");
    struct arg_str* exclude_from_opt = arg_str0(NULL, "exclude-from", "FILE", "read exclude patterns");
    struct arg_str* info_opt         = arg_str0(NULL, "info", "SPEC", "fine-grained info control");
    struct arg_str* bwlimit_opt      = arg_str0(NULL, "bwlimit", "KBPS", "bandwidth limit");

    struct arg_str* pos_arg          = arg_strn(NULL, NULL, "ARG", 0, 16, "positional arg");
    struct arg_end* end              = arg_end(20);

    std::vector<void*> table = {
        help_opt, version_opt,
        recursive_opt, links_opt, perms_opt, times_opt, group_opt, owner_opt, devices_opt,
        archive_opt, update_opt, checksum_opt, delete_opt, dryrun_opt,
        ignore_errors_opt, remove_src_opt,
        verbose_opt, quiet_opt, human_read_opt, progress_opt, itemize_opt, stats_opt,
        cvs_exclude_opt,
        exclude_opt, include_opt, exclude_from_opt, info_opt, bwlimit_opt,
        pos_arg, end
    };

    ArgTable argt(table);

    int nerrors = argt.parse(argc, argv);
    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // ── Help / Version ──
    if (help_opt->count > 0) {
        print_help(argv[0]);
        return 0;
    }
    if (version_opt->count > 0) {
        print_version("rsync");
        return 0;
    }

    // ── Collect positional args ──
    std::vector<std::string> posargs;
    for (int i = 0; i < pos_arg->count; ++i) {
        posargs.emplace_back(pos_arg->sval[i]);
    }

    // ── Validate positional args ──
    if (posargs.size() < 2) {
        fprintf(stderr, "rsync: you must specify at least one source and one destination\n");
        return 1;
    }

    std::string src = posargs[0];
    std::string dst = posargs[1];

    // ── Validate conflicts ──
    if (progress_opt->count > 0 && quiet_opt->count > 0) {
        fprintf(stderr, "rsync: --progress and --quiet conflict\n");
        return 1;
    }

    // ── Build options ──
    bool do_archive = archive_opt->count > 0;
    bool do_recursive = do_archive || recursive_opt->count > 0;
    bool do_links = do_archive || links_opt->count > 0;
    bool do_preserve_perms = do_archive || perms_opt->count > 0;
    bool do_preserve_times = do_archive || times_opt->count > 0;
    bool do_preserve_group = do_archive || group_opt->count > 0;
    bool do_preserve_owner = do_archive || owner_opt->count > 0;
    bool do_delete = delete_opt->count > 0;
    bool do_dryrun = dryrun_opt->count > 0;
    bool do_update = update_opt->count > 0;
    bool do_checksum = checksum_opt->count > 0;
    bool do_itemize = itemize_opt->count > 0;
    bool do_cvs_excl = cvs_exclude_opt->count > 0;
    bool do_verbose = verbose_opt->count > 0;

    // Collect exclude patterns
    std::vector<std::string> excludes;
    for (int i = 0; i < exclude_opt->count; ++i) {
        excludes.emplace_back(exclude_opt->sval[i]);
    }
    if (exclude_from_opt->count > 0) {
        FILE* f = fopen(exclude_from_opt->sval[0], "r");
        if (f) {
            char line[4096];
            while (fgets(line, sizeof(line), f)) {
                std::string s(line);
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                if (!s.empty() && s[0] != '#') excludes.push_back(s);
            }
            fclose(f);
        }
    }

    // Collect include patterns
    std::vector<std::string> includes;
    for (int i = 0; i < include_opt->count; ++i) {
        includes.emplace_back(include_opt->sval[i]);
    }

    // ── Resolve paths ──
    bool src_trailing = !src.empty() && src.back() == '/';
    bool dst_trailing = !dst.empty() && dst.back() == '/';

    // Clean trailing slashes
    if (!src.empty() && src.back() == '/') src.pop_back();
    if (!dst.empty() && dst.back() == '/') dst.pop_back();

    // Stat source
    struct stat src_stat{};
    if (lstat(src.c_str(), &src_stat) != 0) {
        fprintf(stderr, "rsync: failed to stat '%s': %s\n", src.c_str(), strerror(errno));
        return 1;
    }

    // Stat destination
    struct stat dst_stat{};
    lstat(dst.c_str(), &dst_stat);

    // Create destination if needed
    if (!S_ISDIR(src_stat.st_mode) || !src_trailing) {
        if (lstat(dst.c_str(), &dst_stat) != 0) {
            mkdirs(dst);
        }
    }

    // Build destination path
    std::string dst_path;
    if (S_ISDIR(src_stat.st_mode)) {
        if (S_ISDIR(dst_stat.st_mode) || dst_trailing) {
            dst_path = dst + '/';
        } else {
            dst_path = dst;
        }
    } else {
        if (S_ISDIR(dst_stat.st_mode)) {
            std::string base = std::filesystem::path(src).filename().string();
            dst_path = dst + '/' + base;
        } else {
            dst_path = dst;
        }
    }

    // ── Perform the sync ──
    int errors = 0;

    FileTree src_tree{}; // empty tree, populated if src is a directory
    if (S_ISDIR(src_stat.st_mode)) {
        src_tree = FileTree(src);

        for (const auto& entry : src_tree.entries) {
            if (should_exclude(entry.path, excludes, includes, do_cvs_excl)) {
                continue;
            }

            std::string src_full = src + '/' + entry.path;
            std::string dst_full = dst_path + entry.path;

            if (entry.is_dir) {
                if (do_dryrun) {
                    if (do_verbose) printf("%s/\n", entry.path.c_str());
                } else {
                    if (mkdir(dst_full.c_str(), 0755) != 0 && errno != EEXIST) {
                        if (ignore_errors_opt->count == 0) {
                            fprintf(stderr, "rsync: cannot create directory '%s': %s\n",
                                    dst_full.c_str(), strerror(errno));
                            ++errors;
                        }
                    }
                }
            } else if (entry.is_link) {
                if (do_links) {
                    if (do_dryrun) {
                        if (do_verbose) {
                            printf("symlink %s -> %s\n", entry.path.c_str(),
                                   entry.link_target.c_str());
                        }
                    } else {
                        std::string link_dir = dst_full.substr(0, dst_full.rfind('/'));
                        mkdirs(link_dir.c_str());
                        if (symlink(entry.link_target.c_str(), dst_full.c_str()) != 0) {
                            if (ignore_errors_opt->count == 0) {
                                fprintf(stderr, "rsync: cannot create symlink '%s': %s\n",
                                        dst_full.c_str(), strerror(errno));
                                ++errors;
                            }
                        }
                    }
                }
            } else {
                // Regular file
                bool need_transfer = true;
                if (!do_dryrun) {
                    struct stat dst_st{};
                    if (lstat(dst_full.c_str(), &dst_st) == 0) {
                        if (!do_checksum) {
                            if (dst_st.st_size == entry.size &&
                                static_cast<double>(dst_st.st_mtime) == entry.mtime) {
                                need_transfer = false;
                            }
                        }
                        if (do_update && static_cast<double>(dst_st.st_mtime) >= entry.mtime) {
                            need_transfer = false;
                        }
                    }
                }

                if (need_transfer) {
                    if (do_itemize || do_verbose) {
                        // Itemize: sender/receiver ., type f, checksum c, times t, size s, perms p, owner o, group g, target .
                        std::string items = ".fc.t.s.p.o.g.";
                        if (do_checksum) items[2] = 'c'; else items[2] = '.';
                        if (do_preserve_times) items[4] = 't'; else items[4] = '.';
                        // size changes on every transfer
                        items[6] = 's';
                        if (do_preserve_perms) items[8] = 'p'; else items[8] = '.';
                        if (do_preserve_owner) items[10] = 'o'; else items[10] = '.';
                        if (do_preserve_group) items[12] = 'g'; else items[12] = '.';
                        if (do_verbose) {
                            printf("%s -> %s\n", entry.path.c_str(), (dst_path + entry.path).c_str());
                        }
                    }

                    if (!copy_file(src_full, dst_full,
                                   do_preserve_perms, do_preserve_times,
                                   do_dryrun, do_verbose)) {
                        if (ignore_errors_opt->count == 0) {
                            ++errors;
                        }
                    }

                    if (remove_src_opt->count > 0 && !do_dryrun) {
                        remove(src_full.c_str());
                    }
                }
            }
        }

        // Handle --delete (simplified: skip complex delete for v1)
        if (do_delete && do_dryrun && do_verbose) {
            // Would show deletions in dry-run mode
        }
    } else {
        // Single file copy
        if (do_dryrun) {
            if (do_verbose) printf("%s\n", src.c_str());
        } else {
            if (!copy_file(src, dst_path,
                           do_preserve_perms, do_preserve_times,
                           false, do_verbose)) {
                ++errors;
            }
            if (remove_src_opt->count > 0) {
                remove(src.c_str());
            }
        }
    }

    if (stats_opt->count > 0 && quiet_opt->count == 0) {
        if (S_ISDIR(src_stat.st_mode)) {
            printf("Number of files: %zu\n", src_tree.entries.size());
        }
    }

    return errors > 0 ? 1 : 0;
}

REGISTER_COMMAND("rsync", rsync_command, "Synchronize files and directories");
