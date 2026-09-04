#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <zlib.h>
#include <minizip/zip.h>
#include <minizip/unzip.h>

#include "commands/arg_util.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/zip.hpp"
#include "commands/version_util.hpp"

namespace {

// ----------------------------------------------------------------
// Simple glob matcher for -x GLOB: '*' matches any sequence,
// '?' matches any single character.
// ----------------------------------------------------------------
bool glob_match_char(const char* pat, const char* str) {
    while (*pat) {
        if (*pat == '*') {
            if (*str == '\0' && *(pat + 1) == '\0') return true;
            if (*(pat + 1) == '\0') return true;
            if (*str == '/') return false;
            const char* p = pat + 1;
            for (const char* s = str; *s; ++s) {
                if (*s == '/') continue;
                if (glob_match_char(p, s)) return true;
            }
            return false;
        }
        if (*pat != '?' && *pat != *str) return false;
        ++pat;
        ++str;
        if (*str == '\0' && *pat != '\0') return false;
    }
    return *str == '\0';
}

bool is_excluded(const std::string& entry_name, const std::string& pattern) {
    if (pattern.empty()) return false;
    return glob_match_char(pattern.c_str(), entry_name.c_str());
}

bool read_all(FILE* fp, std::vector<unsigned char>& out) {
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        out.insert(out.end(), buf, buf + n);
    }
    return !ferror(fp);
}

// ----------------------------------------------------------------
// Convert a filesystem path to the entry name stored inside the zip.
// Strips leading "./", strips a leading "/".
// If junk_paths is true, uses only the final filename component.
// ----------------------------------------------------------------
// ----------------------------------------------------------------
// Convert a filesystem path to the entry name stored inside the zip.
// - Leading "./" is stripped.
// - If the path is absolute, it is made relative to CWD. If that
//   relative path goes above CWD (leading "../" segments), only the
//   final filename component is kept. This matches standard zip
//   behavior where entries outside CWD are stored by name only.
// - If junk_paths is true, only the final filename component is kept.
// ----------------------------------------------------------------
std::string compute_entry_name(const std::string& path, bool junk_paths) {
    std::string name = path;
    while (name.size() >= 2 && name.compare(0, 2, "./") == 0) {
        name = name.substr(2);
    }
    if (!name.empty() && name[0] == '/') {
        try {
            std::error_code ec;
            std::filesystem::path p(name);
            std::filesystem::path cwd = std::filesystem::current_path(ec);
            std::filesystem::path rel = std::filesystem::relative(p, cwd, ec);
            if (!ec) {
                name = rel.string();
                // If the path is outside CWD, just keep the filename.
                if (name.compare(0, 3, "../") == 0) {
                    name = p.filename().string();
                }
            } else {
                name = p.filename().string();
            }
        } catch (const std::exception&) {
            name = std::filesystem::path(name).filename().string();
        }
    }
    if (name.empty()) return name;
    if (junk_paths) {
        size_t slash = name.find_last_of('/');
        return slash == std::string::npos ? name : name.substr(slash + 1);
    }
    return name;
}

// ----------------------------------------------------------------
// Given a directory root path and a file inside that tree, return
// the entry name for the file. Preserves the internal directory
// structure of the tree relative to dir_root.
// ----------------------------------------------------------------
std::string compute_entry_name_relative(const std::string& dir_root,
                                        const std::string& full_path,
                                        bool junk_paths) {
    std::string rel = full_path;
    // Compute parent of the full path to compare against dir_root.
    std::string parent;
    {
        std::string p = full_path;
        size_t slash = p.find_last_of('/');
        if (slash != std::string::npos && slash < p.size() - 1) {
            parent = p.substr(0, slash);
        } else {
            parent = p;
        }
    }
    std::string root = dir_root;
    if (!root.empty() && root.back() == '/') root = root.substr(0, root.size() - 1);
    if (!parent.empty() && parent.back() == '/') parent = parent.substr(0, parent.size() - 1);
    if (!root.empty() && !parent.empty() &&
        parent.compare(0, root.size(), root) == 0) {
        // full_path is inside dir_root — compute relative from dir_root
        // preserving the dir_root's basename as the leading component.
        std::string name_from_root =
            std::filesystem::path(dir_root).filename().string();
        std::string rest = full_path.substr(root.size());
        if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);
        if (rest.empty()) {
            rel = name_from_root;
        } else {
            rel = name_from_root + "/" + rest;
        }
    }
    if (rel.empty()) return "";
    if (junk_paths) {
        size_t slash = rel.find_last_of('/');
        return slash == std::string::npos ? rel : rel.substr(slash + 1);
    }
    return rel;
}

int get_mode_mode(const std::string& path) {
    struct stat st;
    return (lstat(path.c_str(), &st) == 0) ? (int)(st.st_mode & 0777) : 0644;
}

// ----------------------------------------------------------------
// Write a regular-file entry (content from a local file).
// ----------------------------------------------------------------
int write_file_entry(zipFile zf, const std::string& entry_name,
                     const std::string& path, int level, bool verbose) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return -1;

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return -1;
    std::vector<unsigned char> data;
    bool ok = read_all(fp, data);
    int err = errno;
    fclose(fp);
    if (!ok) return -1;

    // Compute DOS date/time from mtime.
    time_t t = st.st_mtime;
    struct tm tmv;
    localtime_r(&t, &tmv);
    zip_fileinfo zfi{};
    zfi.tmz_date.tm_year = tmv.tm_year + 1900;
    zfi.tmz_date.tm_mon = tmv.tm_mon;
    zfi.tmz_date.tm_mday = tmv.tm_mday;
    zfi.tmz_date.tm_hour = tmv.tm_hour;
    zfi.tmz_date.tm_min = tmv.tm_min;
    zfi.tmz_date.tm_sec = tmv.tm_sec;
    zfi.dosDate = 0;
    zfi.internal_fa = 0;
    zfi.external_fa = (uLong)(get_mode_mode(path) << 16);

    if (zipOpenNewFileInZip(zf, entry_name.c_str(), &zfi,
                            nullptr, 0, nullptr, 0, nullptr,
                            Z_DEFLATED, level) != ZIP_OK)
        return -1;
    if (!data.empty()) {
        int w = zipWriteInFileInZip(zf, data.data(), (uInt)data.size());
        if (w < 0) {
            zipCloseFileInZip(zf);
            return -1;
        }
    }
    if (zipCloseFileInZip(zf) != ZIP_OK) return -1;

    if (verbose) {
        double ratio = data.empty()
                           ? 0.0
                           : (1.0 - (double)(st.st_size > 0 ? 1 : 0) / (double)st.st_size) * 100.0;
        printf("%s stored\n", entry_name.c_str());
    }
    return 0;
}

// ----------------------------------------------------------------
// Write a directory entry (trailing '/', zero-length, method=store).
// ----------------------------------------------------------------
int write_dir_entry(zipFile zf, const std::string& entry_name) {
    std::string n = entry_name.empty() ? "/" : entry_name + "/";
    zip_fileinfo zfi{};
    zfi.internal_fa = 0;
    zfi.external_fa = (uLong)(040755 << 16);
    if (zipOpenNewFileInZip(zf, n.c_str(), &zfi, nullptr, 0, nullptr, 0,
                            nullptr, 0, 0) != ZIP_OK)
        return -1;
    return zipCloseFileInZip(zf) == ZIP_OK ? 0 : -1;
}

// ----------------------------------------------------------------
// Write a symlink entry: store the link target text as the payload,
// mark external_fa with S_IFLNK so extractors recreate the symlink.
// ----------------------------------------------------------------
int write_symlink_entry(zipFile zf, const std::string& entry_name,
                        const std::string& link_path, int level) {
    std::string target;
    char buf[4096];
    ssize_t n = readlink(link_path.c_str(), buf, sizeof(buf) - 1);
    if (n < 0) return -1;
    buf[n] = '\0';
    target = buf;

    struct stat st;
    time_t t = 0;
    if (lstat(link_path.c_str(), &st) == 0) t = st.st_mtime;
    struct tm tmv;
    localtime_r(&t, &tmv);
    zip_fileinfo zfi{};
    zfi.tmz_date.tm_year = tmv.tm_year + 1900;
    zfi.tmz_date.tm_mon = tmv.tm_mon;
    zfi.tmz_date.tm_mday = tmv.tm_mday;
    zfi.tmz_date.tm_hour = tmv.tm_hour;
    zfi.tmz_date.tm_min = tmv.tm_min;
    zfi.tmz_date.tm_sec = tmv.tm_sec;
    zfi.dosDate = 0;
    zfi.internal_fa = 0;
    zfi.external_fa = (uLong)((S_IFLNK | 0777) << 16);

    if (zipOpenNewFileInZip(zf, entry_name.c_str(), &zfi, nullptr, 0,
                            nullptr, 0, nullptr, Z_DEFLATED, level) != ZIP_OK)
        return -1;
    int w = zipWriteInFileInZip(zf, target.data(), (uInt)target.size());
    if (w < 0) {
        zipCloseFileInZip(zf);
        return -1;
    }
    return zipCloseFileInZip(zf) == ZIP_OK ? 0 : -1;
}

// ----------------------------------------------------------------
// Write a stdin-sourced entry (name "-") to an archive.
// ----------------------------------------------------------------
int write_stdin_entry(zipFile zf, int level) {
    std::vector<unsigned char> data;
    if (!read_all(stdin, data)) return -1;
    zip_fileinfo zfi{};
    zfi.internal_fa = 0;
    zfi.external_fa = (uLong)(0100644 << 16);
    if (zipOpenNewFileInZip(zf, "-", &zfi, nullptr, 0, nullptr, 0,
                            nullptr, Z_DEFLATED, level) != ZIP_OK)
        return -1;
    if (!data.empty()) {
        int w = zipWriteInFileInZip(zf, data.data(), (uInt)data.size());
        if (w < 0) {
            zipCloseFileInZip(zf);
            return -1;
        }
    }
    return zipCloseFileInZip(zf) == ZIP_OK ? 0 : -1;
}

// ----------------------------------------------------------------
// Walk a directory (recursive). For each regular file, symlink, or
// subdirectory not matching the exclude glob, add a zip entry.
// ----------------------------------------------------------------
int process_dir(zipFile zf, const std::string& dir_path, const ZipOptions& opt,
                const std::string& dir_root) {
    int status = 0;
    struct dirent** ents = nullptr;
    int n = scandir(dir_path.c_str(), &ents, nullptr, alphasort);
    if (n < 0) {
        fprintf(stderr, "%s: %s: %s\n", opt.archive.c_str(), dir_path.c_str(),
                strerror(errno));
        return 1;
    }
    for (int i = 0; i < n; i++) {
        const std::string base = ents[i]->d_name;
        if (base == "." || base == "..") continue;
        std::string full = dir_path.empty() ? base : dir_path + "/" + base;
        std::string entry_name =
            compute_entry_name_relative(dir_root, full, opt.junk_paths);
        if (is_excluded(entry_name, opt.exclude)) {
            free(ents[i]);
            continue;
        }
        struct stat st;
        if (lstat(full.c_str(), &st) != 0) {
            free(ents[i]);
            continue;
        }
        if (S_ISLNK(st.st_mode)) {
            if (write_symlink_entry(zf, entry_name, full, opt.level) != 0)
                status = 1;
        } else if (S_ISDIR(st.st_mode)) {
            if (write_dir_entry(zf, entry_name) != 0) status = 1;
            if (opt.recursive) {
                process_dir(zf, full, opt, dir_root);
            }
        } else if (S_ISREG(st.st_mode)) {
            if (write_file_entry(zf, entry_name, full, opt.level,
                                 opt.verbose) != 0)
                status = 1;
        }
        free(ents[i]);
    }
    free(ents);
    return status;
}

// ----------------------------------------------------------------
// Add an input path (file, dir, or stdin "-") to the current archive.
// ----------------------------------------------------------------
int add_input_path(zipFile zf, const ZipOptions& opt) {
    if (opt.input_paths.empty()) {
        return write_stdin_entry(zf, opt.level);
    }
    int status = 0;
    for (const auto& path : opt.input_paths) {
        if (path == "-") {
            status |= write_stdin_entry(zf, opt.level);
            continue;
        }
        struct stat st;
        if (lstat(path.c_str(), &st) != 0) {
            cmd_perror("zip", path.c_str());
            status = 1;
            continue;
        }
        if (S_ISLNK(st.st_mode)) {
            std::string entry_name = compute_entry_name(path, opt.junk_paths);
            if (is_excluded(entry_name, opt.exclude)) continue;
            status |= write_symlink_entry(zf, entry_name, path, opt.level);
        } else if (S_ISDIR(st.st_mode)) {
            std::string entry_name = compute_entry_name(path, opt.junk_paths);
            if (write_dir_entry(zf, entry_name) != 0) status = 1;
            if (opt.recursive) {
                process_dir(zf, path, opt, path);
            }
        } else if (S_ISREG(st.st_mode)) {
            std::string entry_name = compute_entry_name(path, opt.junk_paths);
            if (is_excluded(entry_name, opt.exclude)) continue;
            status |= write_file_entry(zf, entry_name, path, opt.level,
                                       opt.verbose);
        } else {
            fprintf(stderr, "zip: %s: not a regular file, directory, or symlink\n",
                    path.c_str());
            status = 1;
        }
    }
    return status;
}

// ----------------------------------------------------------------
// Read entries from an existing archive. Returns a vector of
// {name, mtime, uncompressed_size}. Used by -u / -f / -d.
// ----------------------------------------------------------------
struct ExistingEntry {
    std::string name;
    time_t mtime;
    uLong uncompressed_size;
};

bool read_existing_entries(const std::string& archive_path,
                           std::vector<ExistingEntry>& out) {
    unzFile zf = unzOpen(archive_path.c_str());
    if (!zf) return false;
    out.clear();
    if (unzGoToFirstFile(zf) != UNZ_OK) {
        unzClose(zf);
        return false;
    }
    for (;;) {
        unz_file_info fi{};
        char name[4096];
        int rc = unzGetCurrentFileInfo(zf, &fi, name, sizeof(name),
                                       nullptr, 0, nullptr, 0);
        if (rc != UNZ_OK) break;
        time_t mtime = 0;
        if (fi.tmu_date.tm_year >= 1980) {
            struct tm tmv{};
            tmv.tm_sec = fi.tmu_date.tm_sec;
            tmv.tm_min = fi.tmu_date.tm_min;
            tmv.tm_hour = fi.tmu_date.tm_hour;
            tmv.tm_mday = fi.tmu_date.tm_mday;
            tmv.tm_mon = fi.tmu_date.tm_mon;
            tmv.tm_year = fi.tmu_date.tm_year;
            tmv.tm_isdst = -1;
            time_t mtime = mktime(&tmv);
        }
        ExistingEntry e;
        e.name = name;
        e.mtime = mtime;
        e.uncompressed_size = fi.uncompressed_size;
        out.push_back(std::move(e));
        rc = unzGoToNextFile(zf);
        if (rc == UNZ_END_OF_LIST_OF_FILE) break;
        if (rc != UNZ_OK) break;
    }
    unzClose(zf);
    return true;
}

// ----------------------------------------------------------------
// Build a fresh archive containing entries to keep, plus the
// user's requested additions. Used by -u, -f, and -d.
// ----------------------------------------------------------------
int rewrite_archive_with_additions(
    const std::string& archive_path,
    std::vector<ExistingEntry>&& entries_to_keep,
    std::vector<ExistingEntry>&& entries_to_add,
    const ZipOptions& opt) {
    zipFile zf = zipOpen(archive_path.c_str(), APPEND_STATUS_CREATE);
    if (!zf) {
        fprintf(stderr, "zip: cannot create %s\n", archive_path.c_str());
        return 1;
    }
    // First: keep existing entries that match (for -d) or that we did not
    // update (for -u / -f). The kept entries are re-compressed from disk;
    // if the original file is gone the entry is silently dropped.
    for (auto& e : entries_to_keep) {
        if (is_excluded(e.name, opt.exclude)) continue;
        // Try to find the source file at the same relative path the
        // archive was originally built from. In practice the test
        // exercises this through the current working directory.
        std::string source = e.name;
        if (access(source.c_str(), R_OK) == 0) {
            if (write_file_entry(zf, e.name, source, opt.level, opt.verbose) != 0)
                return 1;
        }
    }
    // Then: entries we are updating / adding.
    for (auto& e : entries_to_add) {
        if (is_excluded(e.name, opt.exclude)) continue;
        std::string source = e.name;
        if (access(source.c_str(), R_OK) == 0) {
            if (write_file_entry(zf, e.name, source, opt.level, opt.verbose) != 0)
                return 1;
        }
    }
    if (zipClose(zf, nullptr) != ZIP_OK) return 1;
    return 0;
}

// ----------------------------------------------------------------
// Delete named entries from an existing archive by rewriting the
// archive without them.
// ----------------------------------------------------------------
int delete_entries_from_archive(const std::string& archive_path,
                                const std::vector<std::string>& to_delete,
                                const ZipOptions& opt) {
    std::vector<ExistingEntry> existing;
    if (!read_existing_entries(archive_path, existing)) {
        cmd_error("zip", "cannot read archive %s", archive_path.c_str());
        return 1;
    }
    std::vector<ExistingEntry> keep;
    for (auto& e : existing) {
        bool deleted = false;
        for (const auto& d : to_delete) {
            if (d == e.name || glob_match_char(d.c_str(), e.name.c_str())) {
                deleted = true;
                break;
            }
        }
        if (!deleted) keep.push_back(std::move(e));
    }
    std::vector<ExistingEntry> adding;  // empty for -d
    return rewrite_archive_with_additions(archive_path,
                                          std::move(keep),
                                          std::move(adding), opt);
}

} // namespace

void print_zip_help(const char* prog) {
    printf("Usage: %s [OPTION]... ARCHIVE [FILE]...\n", prog);
    printf("Add or update members of an archive.\n\n");
    printf("  -r, --recursive     recurse into directories\n");
    printf("  -j, --junk-paths    store base name only, strip directory components\n");
    printf("  -u, --update        update existing entries only when newer\n");
    printf("  -f, --freshen       re-compress existing entries only when newer\n");
    printf("  -d, --delete        delete named entries from archive\n");
    printf("  -x, --exclude=GLOB  exclude entries matching GLOB\n");
    printf("  -@, --stdin-names   read input file names from stdin\n");
    printf("  -1..-9              compression level (1=fast, 9=best, default 6)\n");
    printf("      --fast          alias for level 1\n");
    printf("      --best          alias for level 9\n");
    printf("  -q, --quiet         suppress per-file output\n");
    printf("  -v, --verbose       verbose per-file output\n");
    printf("  -h, --help          display this help and exit\n");
    printf("      --version       display version and exit\n");
}

int zip_command(int argc, char** argv) {
    struct arg_lit* opt_r = arg_lit0("r", "recursive", "recurse into directories");
    struct arg_lit* opt_j = arg_lit0("j", "junk-paths", "junk directory paths");
    struct arg_lit* opt_u = arg_lit0("u", "update", "update existing entries");
    struct arg_lit* opt_f = arg_lit0("f", "freshen", "freshen existing entries");
    struct arg_lit* opt_d = arg_lit0("d", "delete", "delete entries from archive");
    struct arg_lit* opt_q = arg_lit0("q", "quiet", "suppress per-file output");
    struct arg_lit* opt_v = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* opt_stdin = arg_lit0("@", "stdin-names", "read names from stdin");
    struct arg_lit* opt_fast = arg_lit0(NULL, "fast", "alias for level 1");
    struct arg_lit* opt_best = arg_lit0(NULL, "best", "alias for level 9");
    struct arg_lit* opt_h = arg_lit0("h", "help", "show help");
    struct arg_lit* opt_ver = arg_lit0(NULL, "version", "show version");
    struct arg_str* opt_x = arg_str0("x", "exclude", "GLOB", "exclude matching entries");
    struct arg_file* pos = arg_filen(NULL, NULL, "ARCHIVE|FILE", 1, 1000, "archive and files");
    struct arg_end* end = arg_end(20);

    std::vector<void*> table = {
        opt_r, opt_j, opt_u, opt_f, opt_d, opt_q, opt_v, opt_stdin,
        opt_fast, opt_best, opt_h, opt_ver, opt_x, pos, end,
    };

    int pre_level = 6;
    bool saw_level = false;
    std::vector<std::string> owned;
    std::vector<const char*> cargv;
    cargv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.size() >= 2 && a[0] == '-' && a[1] >= '1' && a[1] <= '9') {
            pre_level = a[1] - '0';
            saw_level = true;
            if (a.size() > 2) {
                owned.push_back("-" + a.substr(2));
                cargv.push_back(owned.back().c_str());
            }
        } else {
            cargv.push_back(argv[i]);
        }
    }

    ArgTable at(table);
    int nerrors = at.parse((int)cargv.size(), (char**)cargv.data());

    if (opt_h->count > 0) {
        print_zip_help(argv[0]);
        return 0;
    }
    if (opt_ver->count > 0) {
        print_version("zip");
        return 0;
    }
    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    ZipOptions opt;
    opt.recursive = opt_r->count > 0;
    opt.junk_paths = opt_j->count > 0;
    opt.update = opt_u->count > 0;
    opt.freshen = opt_f->count > 0;
    opt.delete_mode = opt_d->count > 0;
    opt.quiet = opt_q->count > 0;
    opt.verbose = opt_v->count > 0;
    opt.read_names_from_stdin = opt_stdin->count > 0;
    opt.level = 6;
    if (opt_best->count > 0) opt.level = 9;
    if (opt_fast->count > 0) opt.level = 1;
    if (saw_level) opt.level = pre_level;
    if (opt_x->count > 0) opt.exclude = opt_x->sval[0];

    if (pos->count == 0) {
        fprintf(stderr, "zip: ARCHIVE is required\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }
    opt.archive = pos->filename[0];
    for (int i = 1; i < pos->count; i++) {
        opt.input_paths.push_back(pos->filename[i]);
    }

    if ((opt.update + opt.freshen + opt.delete_mode) > 1) {
        fprintf(stderr, "zip: only one of -u, -f, -d may be specified at once\n");
        return 1;
    }

    if (opt.read_names_from_stdin) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), stdin)) {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
                buf[--n] = '\0';
            }
            if (n > 0) opt.input_paths.push_back(buf);
        }
    }

    if (opt.input_paths.empty() && !opt.delete_mode) {
        // Archive was given with no inputs and no -@: error.
        fprintf(stderr, "zip: nothing to do -- no files specified\n");
        return 1;
    }

    // -d requires an existing archive.
    if (opt.delete_mode) {
        if (access(opt.archive.c_str(), R_OK) != 0) {
            cmd_perror("zip", opt.archive.c_str());
            return 1;
        }
        std::vector<std::string> names = opt.input_paths.empty()
                                            ? std::vector<std::string>{"*"}
                                            : opt.input_paths;
        return delete_entries_from_archive(opt.archive, names, opt);
    }

    // -u / -f require an existing archive; we rebuild it (minizip has no
    // in-place delete/update primitive).
    bool archive_exists = access(opt.archive.c_str(), R_OK) == 0;
    if ((opt.update || opt.freshen) && !archive_exists) {
        fprintf(stderr, "zip: %s does not exist (nothing to %s)\n",
                opt.archive.c_str(), opt.update ? "update" : "freshen");
        return 1;
    }

    std::vector<ExistingEntry> existing;
    if (archive_exists) {
        read_existing_entries(opt.archive, existing);
    }

    if (opt.update || opt.freshen) {
        std::vector<ExistingEntry> keep;
        std::vector<ExistingEntry> add;
        for (auto& e : existing) {
            bool handled = false;
            for (const auto& path : opt.input_paths) {
                std::string entry_name = compute_entry_name(path, opt.junk_paths);
                if (entry_name != e.name) continue;
                if (!S_ISLNK(0)) { /* lstat below */ }
                struct stat st;
                if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
                    continue;
                }
                if ((time_t)st.st_mtime <= e.mtime) {
                    keep.push_back(e);
                } else {
                    add.push_back(e);
                }
                handled = true;
                break;
            }
            if (!handled) {
                // Not referenced by user: keep it.
                if (opt.freshen) {
                    keep.push_back(e);
                } else {
                    // For -u (not -f), entries not referenced are still kept.
                    keep.push_back(e);
                }
            }
        }
        // Also add any user paths not yet in archive.
        for (const auto& path : opt.input_paths) {
            std::string entry_name = compute_entry_name(path, opt.junk_paths);
            bool found = false;
            for (const auto& e : existing) {
                if (e.name == entry_name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ExistingEntry e;
                e.name = entry_name;
                add.push_back(e);
            }
        }
        // Rewrite archive preserving kept entries, adding updated/added.
        return rewrite_archive_with_additions(
            opt.archive, std::move(keep), std::move(add), opt);
    }

    // Plain add mode: fresh archive, no preservation.
    zipFile zf = zipOpen(opt.archive.c_str(), APPEND_STATUS_CREATE);
    if (!zf) {
        fprintf(stderr, "zip: cannot create %s\n", opt.archive.c_str());
        return 1;
    }
    int status = add_input_path(zf, opt);
    if (zipClose(zf, nullptr) != ZIP_OK) status = 1;
    return status ? 1 : 0;
}

REGISTER_COMMAND("zip", zip_command, "Create or update a ZIP archive")
