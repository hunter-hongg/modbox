#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sys/types.h>
#include <vector>
#include <string>
#include <sys/statvfs.h>
#include <sys/stat.h>

#include "commands/df.hpp"
#include "commands/command_macros.hpp"
#include "commands/json_stringifier.hpp"

struct FsEntry {
    std::string mount_point;
    std::string fs_type;
    uint64_t block_size;
    uint64_t total_blocks;
    uint64_t free_blocks;
    uint64_t avail_blocks;
    uint64_t total_inodes;
    uint64_t free_inodes;
    uint64_t avail_inodes;
};

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [FILE]...\n", prog);
    printf("Show information about the file system on which each FILE resides,\n");
    printf("or all file systems by default.\n");
    printf("\n");
    printf("  -a, --all            include pseudo, duplicate, inaccessible file systems\n");
    printf("  -B, --block-size=SIZE  scale sizes by SIZE before printing them;\n");
    printf("                         e.g., '-BM' prints sizes in units of 1,048,576 bytes;\n");
    printf("                         see SIZE format below\n");
    printf("  -h, --human-readable  print sizes in powers of 1024 (e.g., 1.0K)\n");
    printf("  -H, --si              print sizes in powers of 1000 (e.g., 1.1K)\n");
    printf("  -i, --inodes          list inode information instead of block usage\n");
    printf("  -k                    like --block-size=1K\n");
    printf("  -m, --megabytes       like --block-size=1M\n");
    printf("  -T, --print-type      print file system type\n");
    printf("      --json            output in JSON format\n");
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n");
    printf("\n");
    printf("SIZE may be (or may be an integer optionally followed by) one of following:\n");
    printf("  KB 1000, K 1024, MB 1000*1000, M 1024*1024, and so on for G, T, P, E, Z, Y.\n");
}

static void print_df_version(const char* prog) {
    printf("df (modbox) 1.0\n");
}

static double scale_value(uint64_t val, int human_si) {
    uint64_t const unit = (human_si != 0) ? 1000ULL : 1024ULL;
    if (val < unit) { return static_cast<double>(val);
}
    double v = static_cast<double>(val);
    const char* suffixes = (human_si != 0) ? "kMGTPEZY" : "KMGTPEZY";
    int idx = 0;
    while (v >= unit && idx < 7) {
        v /= static_cast<double>(unit);
        idx++;
    }
    if (human_si != 0) {
        char buf[64];
        (void)snprintf(buf, sizeof(buf), "%.1f%c", v, suffixes[idx]);
        printf("%8s", buf);
    } else {
        printf("%7.1f%c", v, suffixes[idx]);
    }
    return 0.0;
}

static uint64_t parse_block_size(const char* s) {
    if ((s == nullptr) || ((*s) == 0)) { return 0;
}
    char* endp = nullptr;
    uint64_t const val = strtoull(s, &endp, 10);
    if ((endp == nullptr) || ((*endp) == 0)) { return val;
}
    switch (*endp) {
        case 'K': case 'k': return val * 1024ULL;
        case 'M': case 'm': return val * 1024ULL * 1024ULL;
        case 'G': case 'g': return val * 1024ULL * 1024ULL * 1024ULL;
        case 'T': case 't': return val * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        default: return val;
    }
}

static std::string get_mount_point(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) { return path;
}
    dev_t const dev = st.st_dev;
    std::string cur = path;
    struct stat pst;
    while (true) {
        if (stat(cur.c_str(), &pst) != 0) { break;
}
        if (pst.st_dev != dev) { break;
}
        size_t const pos = cur.find_last_of('/');
        if (pos == 0 || pos == std::string::npos) { break;
}
        cur = cur.substr(0, pos);
    }
    return cur;
}

static bool collect_fs(const char* path, bool human, int human_si, uint64_t block_size_override,
                       bool show_inodes, bool show_type, std::vector<FsEntry>& entries) {
    struct statvfs sv;
    if (statvfs(path, &sv) != 0) { return false;
}

    FsEntry e;
    e.mount_point = get_mount_point(path);
    e.fs_type = "unknown";

    uint64_t const bsize = (block_size_override != 0u) ? block_size_override : sv.f_bsize;
    e.block_size = bsize;
    e.total_blocks = sv.f_blocks;
    e.free_blocks = sv.f_bfree;
    e.avail_blocks = sv.f_bavail;
    e.total_inodes = sv.f_files;
    e.free_inodes = sv.f_ffree;
    e.avail_inodes = sv.f_favail;

    entries.push_back(e);
    return true;
}

int df_command(int argc, char** argv) {
    bool human = false;
    bool si = false;
    bool show_inodes = false;
    bool show_type = false;
    bool all = false;
    bool json_mode = false;
    uint64_t block_size_override = 0;
    std::vector<const char*> paths;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_df_version(argv[0]);
            return 0;
        }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--human-readable") == 0) {
            human = true;
            continue;
        }
        if (strcmp(a, "-H") == 0 || strcmp(a, "--si") == 0) {
            si = true;
            continue;
        }
        if (strcmp(a, "-i") == 0 || strcmp(a, "--inodes") == 0) {
            show_inodes = true;
            continue;
        }
        if (strcmp(a, "-T") == 0 || strcmp(a, "--print-type") == 0) {
            show_type = true;
            continue;
        }
        if (strcmp(a, "-a") == 0 || strcmp(a, "--all") == 0) {
            all = true;
            continue;
        }
        if (strncmp(a, "-B", 2) == 0) {
            if (a[2] != '\0') {
                block_size_override = parse_block_size(a + 2);
            } else {
                i++;
                if (i < argc) { block_size_override = parse_block_size(argv[i]);
}
            }
            continue;
        }
        if (strcmp(a, "--block-size") == 0) {
            i++;
            if (i < argc) { block_size_override = parse_block_size(argv[i]);
}
            continue;
        }
        if (strcmp(a, "-k") == 0) {
            block_size_override = 1024ULL;
            continue;
        }
        if (strcmp(a, "-m") == 0 || strcmp(a, "--megabytes") == 0) {
            block_size_override = 1024ULL * 1024ULL;
            continue;
        }
        if (strcmp(a, "--json") == 0) {
            json_mode = true;
            continue;
        }
        if (a[0] == '-') {
            (void)fprintf(stderr, "df: invalid option '%s'\n", a);
            (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 0;
        }
        paths.push_back(a);
    }

    if (paths.empty()) {
        paths.push_back(".");
    }

    if (!json_mode) {
        printf("%-20s ", "Filesystem");
        if (show_type) { printf("%-12s ", "Type");
}
        if (!show_inodes) {
            if (human || si || (block_size_override != 0u)) {
                printf("  Size    Used  Avail Use%%");
            } else {
                printf("  1K-blocks    Used  Available Use%%");
            }
        } else {
            printf("   Inodes   IUsed  IFree IUse%%");
        }
        printf("  Mounted on\n");
    }

    std::vector<FsEntry> json_entries;
    for (size_t p = 0; p < paths.size(); p++) {
        std::vector<FsEntry> entries;
        if (!collect_fs(paths[p], human, static_cast<int>(si), block_size_override, show_inodes, show_type, entries)) {
            (void)fprintf(stderr, "df: %s: %s\n", paths[p], strerror(errno));
            continue;
        }

        for (const auto& e : entries) {
            if (json_mode) {
                json_entries.push_back(e);
                continue;
            }
            printf("%-20s ", e.mount_point.c_str());
            if (show_type) { printf("%-12s ", e.fs_type.c_str());
}

            if (!show_inodes) {
                uint64_t const size = e.total_blocks * (e.block_size / 512);
                uint64_t const used = (e.total_blocks - e.free_blocks) * (e.block_size / 512);
                uint64_t const avail = e.avail_blocks * (e.block_size / 512);
                if (human || si) {
                    printf("  ");
                    scale_value(size, static_cast<int>(si));
                    printf("  ");
                    scale_value(used, static_cast<int>(si));
                    printf("  ");
                    scale_value(avail, static_cast<int>(si));
                    printf(" ");
                } else {
                    printf(" %10llu %7llu %10llu ",
                           static_cast<unsigned long long>(size) / 2,
                           static_cast<unsigned long long>(used) / 2,
                           static_cast<unsigned long long>(avail) / 2);
                }
                if (size > 0) {
                    int const use_pct = static_cast<int>((used * 100) / size);
                    printf("%3d%%", use_pct);
                } else {
                    printf("  -%%");
                }
            } else {
                uint64_t const total = e.total_inodes;
                uint64_t const used = e.total_inodes - e.free_inodes;
                uint64_t const avail = e.avail_inodes;
                printf(" %10llu %7llu %7llu ",
                       static_cast<unsigned long long>(total),
                       static_cast<unsigned long long>(used),
                       static_cast<unsigned long long>(avail));
                if (total > 0) {
                    int const use_pct = static_cast<int>((used * 100) / total);
                    printf("%3d%%", use_pct);
                } else {
                    printf("  -%%");
                }
            }

            printf("  %s\n", e.mount_point.c_str());
        }
    }

    if (json_mode) {
        (void)fprintf(stdout, "[\n");
        for (size_t i = 0; i < json_entries.size(); i++) {
            const FsEntry& e = json_entries[i];
            (void)fprintf(stdout, "  {\n");
            (void)fprintf(stdout, "    \"mount_point\": ");
            json_escape_string(stdout, e.mount_point.c_str());
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"fs_type\": ");
            json_escape_string(stdout, e.fs_type.c_str());
            (void)fprintf(stdout, ",\n");
            (void)fprintf(stdout, "    \"block_size\": %llu,\n", static_cast<unsigned long long>(e.block_size));
            (void)fprintf(stdout, "    \"total_blocks\": %llu,\n", static_cast<unsigned long long>(e.total_blocks));
            (void)fprintf(stdout, "    \"free_blocks\": %llu,\n", static_cast<unsigned long long>(e.free_blocks));
            (void)fprintf(stdout, "    \"avail_blocks\": %llu,\n", static_cast<unsigned long long>(e.avail_blocks));
            (void)fprintf(stdout, "    \"total_inodes\": %llu,\n", static_cast<unsigned long long>(e.total_inodes));
            (void)fprintf(stdout, "    \"free_inodes\": %llu,\n", static_cast<unsigned long long>(e.free_inodes));
            (void)fprintf(stdout, "    \"avail_inodes\": %llu\n", static_cast<unsigned long long>(e.avail_inodes));
            (void)fprintf(stdout, "  }%s\n", (i + 1 < json_entries.size()) ? "," : "");
        }
        (void)fprintf(stdout, "]\n");
    }
    return 0;
}

REGISTER_COMMAND("df", df_command, "Report filesystem disk space usage");
