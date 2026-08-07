#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <sys/mount.h>
#include <unistd.h>

#include "commands/umount.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [-l|--lazy] [-f|--force] [--fake] [device|directory]\n", prog);
    printf("Unmount a filesystem.\n");
    printf("\n");
    printf("  -l, --lazy           lazy unmount: detach immediately, clean up when idle\n");
    printf("  -f, --force          force unmount (e.g. for unreachable NFS)\n");
    printf("      --fake           dry-run: print what would be done without executing\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
}

int umount_command(int argc, char** argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    bool lazy = false;
    bool force = false;
    bool fake = false;
    std::string target;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("umount");
            return 0;
        }
        if (strcmp(a, "-l") == 0 || strcmp(a, "--lazy") == 0) {
            lazy = true;
        } else if (strcmp(a, "-f") == 0 || strcmp(a, "--force") == 0) {
            force = true;
        } else if (strcmp(a, "--fake") == 0) {
            fake = true;
        } else if (a[0] != '-') {
            target = a;
        } else {
            fprintf(stderr, "umount: invalid option '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    if (target.empty()) {
        fprintf(stderr, "umount: missing device or directory operand\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    int flags = 0;
    if (lazy) flags |= MNT_DETACH;
    if (force) flags |= MNT_FORCE;

    if (fake) {
        printf("umount %s\n", target.c_str());
        return 0;
    }

    int ret = umount2(target.c_str(), flags);
    if (ret != 0) {
        if (errno == EINVAL) {
            fprintf(stderr, "umount: %s: not mounted\n", target.c_str());
        } else if (errno == EPERM || errno == EACCES) {
            fprintf(stderr, "umount: %s: %s\n", target.c_str(),
                    (errno == EPERM) ? "Operation not permitted" : "Permission denied");
        } else {
            fprintf(stderr, "umount: %s: %s\n", target.c_str(), strerror(errno));
        }
        return 1;
    }
    return 0;
}

REGISTER_COMMAND("umount", umount_command, "Unmount a filesystem");
