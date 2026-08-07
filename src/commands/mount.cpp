#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/mount.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

struct MountEntry {
    std::string source{};
    std::string target{};
    std::string fstype{};
    std::string options{};
    unsigned int dump{0};
    unsigned int passno{0};
};

// ── /proc/mounts parsing ───────────────────────────────────────────────────

static std::vector<MountEntry> read_proc_mounts() {
    std::vector<MountEntry> entries;
    std::ifstream f("/proc/mounts");
    if (!f) return entries;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        MountEntry e;
        std::istringstream iss(line);
        iss >> e.source >> e.target >> e.fstype >> e.options;
        iss >> e.dump >> e.passno;
        entries.push_back(std::move(e));
    }
    return entries;
}

static void print_help(const char* prog) {
    printf("Usage: %s [-a|--all] [--fake] [-t fstype] [-o options] [--target dir] [device [dir [type [options]]]]\n", prog);
    printf("Mount a filesystem or list mounted filesystems.\n");
    printf("\n");
    printf("  -a, --all            list all currently mounted filesystems\n");
    printf("      --fake           dry-run: print what would be done without executing\n");
    printf("  -t, --type fstype    filesystem type (e.g. ext4, tmpfs, bind)\n");
    printf("  -o, --options opts   comma-separated mount options\n");
    printf("      --target dir     explicit mount point directory\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
}

static int parse_mount_options(const std::string& opt_str, int& flags, std::string& data) {
    size_t pos = 0;
    while (pos < opt_str.size()) {
        size_t comma = opt_str.find(',', pos);
        std::string opt = (comma == std::string::npos)
            ? opt_str.substr(pos)
            : opt_str.substr(pos, comma - pos);

        if (opt == "bind") flags |= MS_BIND;
        else if (opt == "remount") flags |= MS_REMOUNT;
        else if (opt == "rw") flags &= ~MS_RDONLY;
        else if (opt == "ro") flags |= MS_RDONLY;
        else if (!opt.empty()) {
            if (!data.empty()) data += ",";
            data += opt;
        }
        
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return 0;
}

int mount_command(int argc, char** argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    bool list_all = false;
    bool fake = false;
    std::string fstype;
    std::string options;
    std::string target;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("mount");
            return 0;
        }
        if (strcmp(a, "-a") == 0 || strcmp(a, "--all") == 0) {
            list_all = true;
        } else if (strcmp(a, "--fake") == 0) {
            fake = true;
        } else if (strcmp(a, "-t") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "mount: option '-t' requires an argument\n");
                return 1;
            }
            fstype = argv[i];
        } else if (strncmp(a, "-t", 2) == 0) {
            fstype = a + 2;
        } else if (strcmp(a, "-o") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "mount: option '-o' requires an argument\n");
                return 1;
            }
            options = argv[i];
        } else if (strncmp(a, "-o", 2) == 0) {
            options = a + 2;
        } else if (strcmp(a, "--options") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "mount: option '--options' requires an argument\n");
                return 1;
            }
            options = argv[i];
        } else if (strncmp(a, "--options=", 10) == 0) {
            options = a + 10;
        } else if (strcmp(a, "--target") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "mount: option '--target' requires an argument\n");
                return 1;
            }
            target = argv[i];
        } else if (strncmp(a, "--target=", 9) == 0) {
            target = a + 9;
        } else if (a[0] != '-') {
            positional.push_back(a);
        } else {
            fprintf(stderr, "mount: invalid option '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    // List mode
    if (list_all) {
        auto entries = read_proc_mounts();
        if (entries.empty()) {
            fprintf(stderr, "mount: /proc/mounts: No such file or directory\n");
            return 1;
        }
        printf("%-25s %-30s %-12s %-30s %s %s\n",
               "source", "target", "fstype", "options", "dump", "pass");
        for (const auto& e : entries) {
            printf("%-25s %-30s %-12s %-30s %u %u\n",
                   e.source.c_str(), e.target.c_str(), e.fstype.c_str(),
                   e.options.c_str(), e.dump, e.passno);
        }
        return 0;
    }

    // Parse positional args: device [dir [type [options...]]]
    std::string device;
    if (!positional.empty()) {
        device = positional[0];
        if (positional.size() > 1) {
            if (target.empty()) {
                target = positional[1];
            }
            if (fstype.empty() && positional.size() > 2) {
                fstype = positional[2];
            }
            if (options.empty() && positional.size() > 3) {
                // Join remaining positional args as options
                for (size_t j = 3; j < positional.size(); j++) {
                    if (j > 3) options += ",";
                    options += positional[j];
                }
            }
        }
    }

    if (device.empty()) {
        fprintf(stderr, "mount: missing device operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    if (target.empty()) {
        fprintf(stderr, "mount: missing target directory\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    // Check target directory exists (non-fake mode)
    if (!fake) {
        struct stat st;
        if (stat(target.c_str(), &st) != 0) {
            fprintf(stderr, "mount: %s: No such file or directory\n", target.c_str());
            return 1;
        }
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "mount: %s: Not a directory\n", target.c_str());
            return 1;
        }
    }

    // Build flags and data
    int flags = 0;
    std::string data;
    parse_mount_options(options, flags, data);

    const char* fstype_c = fstype.empty() ? nullptr : fstype.c_str();
    const char* data_c = data.empty() ? nullptr : data.c_str();

    if (fake) {
        printf("mount %s on %s type %s", device.c_str(), target.c_str(),
               fstype_c ? fstype_c : "auto");
        if (!options.empty()) {
            printf(" (%s)", options.c_str());
        }
        printf("\n");
        return 0;
    }

    int ret = mount(device.c_str(), target.c_str(), fstype_c, flags, data_c);
    if (ret != 0) {
        if (errno == EPERM || errno == EACCES) {
            fprintf(stderr, "mount: operation not permitted\n");
        } else {
            fprintf(stderr, "mount: %s\n", strerror(errno));
        }
        return 1;
    }
    return 0;
}

REGISTER_COMMAND("mount", mount_command, "Mount a filesystem");
