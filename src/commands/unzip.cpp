#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <zlib.h>
#include <minizip/unzip.h>

#include <argtable3.h>

#include "commands/arg_util.hpp"
#include "commands/cmd_error.hpp"
#include "commands/command_macros.hpp"
#include "commands/unzip.hpp"
#include "commands/version_util.hpp"

namespace {

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.ends_with(suffix);
}

// ----------------------------------------------------------------
// Recursively create directories along path, like mkdir -p.
// ----------------------------------------------------------------
int mkdir_p(const std::string& path, mode_t mode) {
    if (path.empty()) { return 0;
}
    if (mkdir(path.c_str(), mode) == 0) { return 0;
}
    if (errno == EEXIST) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { return 0;
}
        return -1;
    }
    // Recurse on parent.
    size_t const slash = path.find_last_of('/');
    if (slash == 0) { return 0;
}
    if (slash == std::string::npos) { return 0;
}
    if (mkdir_p(path.substr(0, slash), mode) != 0) { return -1;
}
    if (mkdir(path.c_str(), mode) == 0) { return 0;
}
    if (errno == EEXIST) { return 0;
}
    return -1;
}

// ----------------------------------------------------------------
// Strip trailing "/" from an internal path used as a directory key.
// ----------------------------------------------------------------
std::string strip_trailing_slash(const std::string& s) {
    if (!s.empty() && s.back() == '/') { return s.substr(0, s.size() - 1);
}
    return s;
}

// ----------------------------------------------------------------
// List archive contents (unzip -l).
// ----------------------------------------------------------------
int list_archive(const std::string& archive_path, const UnzipOptions& opt) {
    unzFile zf = unzOpen(archive_path.c_str());
    if (zf == nullptr) {
        struct stat st;
        if (stat(archive_path.c_str(), &st) != 0) {
            cmd_perror("unzip", archive_path.c_str());
        } else {
            cmd_error("unzip", "not a valid zip archive: %s", archive_path.c_str());
        }
        return 1;
    }
    unz_global_info64 gi{};
    if (unzGetGlobalInfo64(zf, &gi) != UNZ_OK) {
        unzClose(zf);
        cmd_error("unzip", "not a valid zip archive: %s", archive_path.c_str());
        return 1;
    }
    printf(
        "  Length      Date    Time    Name\n"
        "---------  ---------- -----   ----\n");

    std::vector<unsigned long> lengths;
    if (unzGoToFirstFile(zf) != UNZ_OK) {
        unzClose(zf);
        return 1;
    }
    for (;;) {
        unz_file_info fi{};
        char name[4096];
        int rc = unzGetCurrentFileInfo(zf, &fi, name, sizeof(name),
                                       nullptr, 0, nullptr, 0);
        if (rc != UNZ_OK) { break;
}
        printf("%10lu  %04d-%02d-%02d %02d:%02d:%02d   %s\n",
               static_cast<unsigned long>(fi.uncompressed_size),
               fi.tmu_date.tm_year, fi.tmu_date.tm_mon + 1,
               fi.tmu_date.tm_mday,
               fi.tmu_date.tm_hour, fi.tmu_date.tm_min,
               fi.tmu_date.tm_sec,
               name);
        lengths.push_back(static_cast<unsigned long>(fi.uncompressed_size));
        rc = unzGoToNextFile(zf);
        if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
        if (rc != UNZ_OK) { break;
}
    }
    unsigned long total = 0;
    for (unsigned long l : lengths) { total += l;
}
    printf("---------                     -------\n");
    printf("%10lu                     %5lu files\n",
           total, static_cast<unsigned long>(lengths.size()));
    unzClose(zf);
    return 0;
}

// ----------------------------------------------------------------
// Test archive integrity (unzip -t): open each entry, read it all,
// close it. Check the returned error code (UNZ_CRCERROR etc.).
// ----------------------------------------------------------------
int test_archive(const std::string& archive_path, const UnzipOptions& opt) {
    unzFile zf = unzOpen(archive_path.c_str());
    if (zf == nullptr) {
        struct stat st;
        if (stat(archive_path.c_str(), &st) != 0) {
            cmd_perror("unzip", archive_path.c_str());
        } else {
            cmd_error("unzip", "not a valid zip archive: %s", archive_path.c_str());
        }
        return 1;
    }
    unz_global_info64 gi{};
    if (unzGetGlobalInfo64(zf, &gi) != UNZ_OK) {
        unzClose(zf);
        cmd_error("unzip", "not a valid zip archive: %s", archive_path.c_str());
        return 1;
    }

    printf("%s:  %lu files, %lu bytes\n",
           archive_path.c_str(), static_cast<unsigned long>(gi.number_entry), 0UL);

    int status = 0;
    if (unzGoToFirstFile(zf) != UNZ_OK) {
        unzClose(zf);
        return 1;
    }
    for (;;) {
        unz_file_info fi{};
        char name[4096];
        int rc = unzGetCurrentFileInfo(zf, &fi, name, sizeof(name),
                                       nullptr, 0, nullptr, 0);
        if (rc != UNZ_OK) { break;
}

        int const open_rc = unzOpenCurrentFile(zf);
        if (open_rc != UNZ_OK) {
            (void)fprintf(stderr, "unzip: %s: error %d reading entry\n", name,
                    open_rc);
            status = 1;
            // Skip to next without draining.
            unzGoToNextFile(zf);
            continue;
        }
        unsigned char buf[65536];
        for (;;) {
            int const n = unzReadCurrentFile(zf, buf, sizeof(buf));
            if (n <= 0) { break;
}
            (void)n;
        }
        int const close_rc = unzCloseCurrentFile(zf);
        if (close_rc == UNZ_CRCERROR) {
            (void)fprintf(stderr, "unzip: %s: CRC failed\n", name);
            status = 1;
        } else if (close_rc != UNZ_OK) {
            (void)fprintf(stderr, "unzip: %s: read error %d\n", name, close_rc);
            status = 1;
        } else {
            if (!opt.quiet) { printf("  %s: OK\n", name);
}
        }
        rc = unzGoToNextFile(zf);
        if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
        if (rc != UNZ_OK) { break;
}
    }
    unzClose(zf);
    return status;
}

// ----------------------------------------------------------------
// Print a single entry's uncompressed bytes to stdout (unzip -p).
// ----------------------------------------------------------------
int print_entry(const std::string& archive_path,
                const std::string& entry_name, const UnzipOptions& opt) {
    unzFile zf = unzOpen(archive_path.c_str());
    if (zf == nullptr) {
        struct stat st;
        if (stat(archive_path.c_str(), &st) != 0) {
            cmd_perror("unzip", archive_path.c_str());
        } else {
            cmd_error("unzip", "not a valid zip archive: %s", archive_path.c_str());
        }
        return 1;
    }
    int rc = unzLocateFile(zf, entry_name.c_str(), 0);
    if (rc != UNZ_OK) {
        (void)fprintf(stderr, "unzip: %s: '%s' not found in %s\n",
                archive_path.c_str(), entry_name.c_str(), archive_path.c_str());
        unzClose(zf);
        return 1;
    }
    rc = unzOpenCurrentFile(zf);
    if (rc != UNZ_OK) {
        unzClose(zf);
        return 1;
    }
    unsigned char buf[65536];
    int n;
    while ((n = unzReadCurrentFile(zf, buf, sizeof(buf))) > 0) {
        if (fwrite(buf, 1, static_cast<size_t>(n), stdout) != static_cast<size_t>(n)) {
            (void)fprintf(stderr, "unzip: %s: write error: %s\n",
                    archive_path.c_str(), strerror(errno));
            unzCloseCurrentFile(zf);
            unzClose(zf);
            return 1;
        }
    }
    int const close_rc = unzCloseCurrentFile(zf);
    unzClose(zf);
    if (close_rc == UNZ_CRCERROR) {
        (void)fprintf(stderr, "unzip: %s: CRC failed\n", entry_name.c_str());
        return 1;
    }
    if (close_rc != UNZ_OK) { return 1;
}
    return 0;
}

// ----------------------------------------------------------------
// Extract an archive: for each entry that matches entry_names (if
// specified), write the entry to <target_dir>/<internal_path>.
// Returns 1 if any entry failed; 0 otherwise.
// ----------------------------------------------------------------
int extract_archive(const std::string& archive_path,
                    const UnzipOptions& opt) {
    unzFile zf = unzOpen(archive_path.c_str());
    if (zf == nullptr) {
        // Check if file exists to distinguish "not found" from "bad zip".
        struct stat st;
        if (stat(archive_path.c_str(), &st) != 0) {
            cmd_perror("unzip", archive_path.c_str());
        } else {
            cmd_error("unzip", "not a valid zip archive: %s", archive_path.c_str());
        }
        return 1;
    }
    if (!opt.target_dir.empty()) {
        if (mkdir_p(opt.target_dir, 0755) != 0) {
            (void)fprintf(stderr, "unzip: cannot create directory %s: %s\n",
                    opt.target_dir.c_str(), strerror(errno));
            unzClose(zf);
            return 1;
        }
    }
    int status = 0;
    if (opt.verbose) {
        printf("Archive:  %s\n", archive_path.c_str());
        if (!opt.target_dir.empty()) {
            printf("Replacing with: %s/\n", opt.target_dir.c_str());
}
    }

    if (unzGoToFirstFile(zf) != UNZ_OK) {
        unzClose(zf);
        return 1;
    }
    for (;;) {
        unz_file_info fi{};
        char name[4096];
        int rc = unzGetCurrentFileInfo(zf, &fi, name, sizeof(name),
                                       nullptr, 0, nullptr, 0);
        if (rc != UNZ_OK) { break;
}

        // Entry-name filter: if opt.entry_names is non-empty, only
        // entries whose internal name exactly matches one of the
        // filters are extracted.
        bool wanted = true;
        if (!opt.entry_names.empty()) {
            wanted = false;
            for (const auto& en : opt.entry_names) {
                if (en == name) {
                    wanted = true;
                    break;
                }
            }
        }
        if (!wanted) {
            rc = unzGoToNextFile(zf);
            if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
            if (rc != UNZ_OK) { break;
}
            continue;
        }

        // Skip entries that look like directory placeholders
        // (trailing "/") — create the directory rather than writing
        // an empty file; this makes `-d` + nested archives work
        // cleanly.
        bool const is_dir = (name[0] == 0) || ends_with(name, "/");
        std::string const target =
            opt.target_dir.empty() ? name : opt.target_dir + "/" + name;

        if (is_dir) {
            std::string const d = strip_trailing_slash(target);
            if (!d.empty() && mkdir_p(d, 0755) != 0) {
                (void)fprintf(stderr, "unzip: cannot create directory %s: %s\n",
                        d.c_str(), strerror(errno));
                status = 1;
            }
            rc = unzGoToNextFile(zf);
            if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
            if (rc != UNZ_OK) { break;
}
            continue;
        }

        // Ensure the parent directory exists.
        std::string const parent = target;
        size_t const slash = parent.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
            std::string const p = parent.substr(0, slash);
            if (mkdir_p(p, 0755) != 0) {
                (void)fprintf(stderr, "unzip: cannot create directory %s: %s\n",
                        p.c_str(), strerror(errno));
                status = 1;
                rc = unzGoToNextFile(zf);
                if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
                if (rc != UNZ_OK) { break;
}
                continue;
            }
        }

        // Overwrite policy: default is to skip existing (non-interactive).
        // -o overrides to always overwrite; -n never overwrites.
        if (!opt.overwrite) {
            struct stat st;
            if (stat(target.c_str(), &st) == 0) {
                if (!opt.no_clobber) {
                    if (!opt.quiet) { printf("%s: exists; skipped\n", name);
}
                    rc = unzGoToNextFile(zf);
                    if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
                    if (rc != UNZ_OK) { break;
}
                    continue;
                }
            }
        }

        int const open_rc = unzOpenCurrentFile(zf);
        if (open_rc != UNZ_OK) {
            (void)fprintf(stderr, "unzip: %s: error %d opening entry\n", name,
                    open_rc);
            status = 1;
            rc = unzGoToNextFile(zf);
            if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
            if (rc != UNZ_OK) { break;
}
            continue;
        }
        FILE* out = fopen(target.c_str(), "wb");
        if (out == nullptr) {
            (void)fprintf(stderr, "unzip: %s: %s\n", target.c_str(),
                    strerror(errno));
            unzCloseCurrentFile(zf);
            status = 1;
            rc = unzGoToNextFile(zf);
            if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
            if (rc != UNZ_OK) { break;
}
            continue;
        }
        unsigned char buf[65536];
        for (;;) {
            int const n = unzReadCurrentFile(zf, buf, sizeof(buf));
            if (n <= 0) { break;
}
            if (fwrite(buf, 1, static_cast<size_t>(n), out) != static_cast<size_t>(n)) {
                (void)fprintf(stderr, "unzip: %s: write error: %s\n",
                        target.c_str(), strerror(errno));
                status = 1;
                break;
            }
        }
        (void)fclose(out);
        int const close_rc = unzCloseCurrentFile(zf);
        if (close_rc == UNZ_CRCERROR) {
            (void)fprintf(stderr, "unzip: %s: CRC failed\n", name);
            status = 1;
        } else if (close_rc != UNZ_OK) {
            (void)fprintf(stderr, "unzip: %s: read error %d\n", name, close_rc);
            status = 1;
        } else {
            if (!opt.quiet) { printf("%s\n", name);
}
        }
        rc = unzGoToNextFile(zf);
        if (rc == UNZ_END_OF_LIST_OF_FILE) { break;
}
        if (rc != UNZ_OK) { break;
}
    }
    unzClose(zf);
    return status;
}

} // namespace

void print_unzip_help(const char* prog) {
    printf("Usage: %s [OPTION]... ARCHIVE [FILE]...\n", prog);
    printf("List, extract, or test a ZIP archive.\n\n");
    printf("  -l, --list         list archive contents; do not extract\n");
    printf("  -t, --test         test archive integrity (verify CRCs)\n");
    printf("  -p, --stdout       write entry content to stdout\n");
    printf("  -d, --dir=PATH     extract into PATH\n");
    printf("  -n, --no-clobber   never overwrite existing files\n");
    printf("  -o, --overwrite    always overwrite existing files\n");
    printf("  -q, --quiet        suppress per-file output\n");
    printf("  -v, --verbose      verbose output\n");
    printf("  -h, --help         display this help and exit\n");
    printf("      --version      display version and exit\n");
    printf("\n");
    printf("With no FILE, all entries are acted upon.\n");
}

int unzip_command(int argc, char** argv) {
    struct arg_lit* opt_l = arg_lit0("l", "list", "list archive contents");
    struct arg_lit* opt_t = arg_lit0("t", "test", "test archive integrity");
    struct arg_lit* opt_p = arg_lit0("p", "stdout", "write entry to stdout");
    struct arg_lit* opt_n = arg_lit0("n", "no-clobber", "never overwrite");
    struct arg_lit* opt_o = arg_lit0("o", "overwrite", "always overwrite");
    struct arg_lit* opt_q = arg_lit0("q", "quiet", "suppress per-file output");
    struct arg_lit* opt_v = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* opt_h = arg_lit0("h", "help", "show help");
    struct arg_lit* opt_ver = arg_lit0(NULL, "version", "show version");
    struct arg_str* opt_d = arg_str0("d", "dir", "PATH", "extract into PATH");
    struct arg_file* pos = arg_filen(NULL, NULL, "ARCHIVE|FILE", 1, 1000,
                                     "archive and entries");
    struct arg_end* end = arg_end(20);

    std::vector<void*> const table = {
        opt_l, opt_t, opt_p, opt_n, opt_o, opt_q, opt_v, opt_h, opt_ver,
        opt_d, pos, end,
    };

    ArgTable at(table);
    int const nerrors = at.parse(argc, argv);

    if (opt_h->count > 0) {
        print_unzip_help(argv[0]);
        return 0;
    }
    if (opt_ver->count > 0) {
        print_version("unzip");
        return 0;
    }
    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    UnzipOptions opt;
    opt.list = opt_l->count > 0;
    opt.test = opt_t->count > 0;
    opt.stdout_mode = opt_p->count > 0;
    opt.no_clobber = opt_n->count > 0;
    opt.overwrite = opt_o->count > 0;
    opt.quiet = opt_q->count > 0;
    opt.verbose = opt_v->count > 0;
    if (opt_d->count > 0) { opt.target_dir = opt_d->sval[0];
}

    if (pos->count == 0) {
        (void)fprintf(stderr, "unzip: ARCHIVE is required\n");
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // Convention: the last positional is the archive; earlier ones are
    // entry-name filters. A single positional is always the archive.
    if (pos->count == 1) {
        opt.archive = pos->filename[0];
    } else {
        for (int i = 0; i < pos->count - 1; i++) {
            opt.entry_names.push_back(pos->filename[i]);
        }
        opt.archive = pos->filename[pos->count - 1];
    }

    if (access(opt.archive.c_str(), R_OK) != 0) {
        cmd_perror("unzip", opt.archive.c_str());
        return 1;
    }

    if (opt.list) {
        return list_archive(opt.archive, opt);
    }
    if (opt.test) {
        return test_archive(opt.archive, opt);
    }
    if (opt.stdout_mode) {
        // -p requires exactly one entry name. Use the archive name only
        // if the user happened to supply just one positional (the entry
        // in that case would need to be a name filter, which requires at
        // least 2 positionals). This keeps the CLI simple.
        if (opt.entry_names.empty()) {
            (void)fprintf(stderr, "unzip: -p requires an entry name\n");
            return 1;
        }
        return print_entry(opt.archive, opt.entry_names[0], opt);
    }
    return extract_archive(opt.archive, opt);
}

REGISTER_COMMAND("unzip", unzip_command,
                 "List, extract, or test a ZIP archive")
