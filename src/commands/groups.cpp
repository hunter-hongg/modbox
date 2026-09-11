#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <vector>
#include <string>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include "commands/groups.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [USERNAME]...\n", prog);
    printf("Print group memberships for each USERNAME or for the current process.\n");
    printf("\n");
    printf("      --help        display this help and exit\n");
    printf("      --version     output version information and exit\n");
}

static void print_groups_version(const char* prog) {
    printf("groups (modbox) 1.0\n");
}

static std::string gid_to_name(gid_t gid) {
    const struct group* gr = getgrgid(gid);
    if (gr != nullptr) { return std::string(gr->gr_name);
}
    char buf[32];
    (void)snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(gid));
    return std::string(buf);
}

static std::vector<gid_t> get_user_groups(const char* username) {
    std::vector<gid_t> groups;
    const struct passwd* pw = getpwnam((username != nullptr) ? username : "");
    if (pw == nullptr) {
        pw = getpwuid(getuid());
    }
    if (pw == nullptr) { return groups;
}

    int ngroups = 32;
    gid_t* gids = static_cast<gid_t*>(malloc(static_cast<size_t>(ngroups) * sizeof(gid_t)));
    if (gids == nullptr) { return groups;
}

    int const ret = getgrouplist(pw->pw_name, pw->pw_gid, gids, &ngroups);
    if (ret >= 0) {
        for (int i = 0; i < ngroups; i++) {
            groups.push_back(gids[i]);
        }
    }
    free(gids);
    return groups;
}

int groups_command(int argc, char** argv) {
    std::vector<const char*> usernames;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_groups_version(argv[0]);
            return 0;
        }
        if (a[0] == '-') {
            (void)fprintf(stderr, "groups: invalid option '%s'\n", a);
            (void)fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 0;
        }
        usernames.push_back(a);
    }

    if (usernames.empty()) {
        usernames.push_back(nullptr);
    }

    for (size_t u = 0; u < usernames.size(); u++) {
        std::vector<gid_t> const gids = get_user_groups(usernames[u]);
        bool first = true;
        for (gid_t gid : gids) {
            if (!first) { printf(" ");
}
            printf("%s", gid_to_name(gid).c_str());
            first = false;
        }
        printf("\n");
    }
    return 0;
}

REGISTER_COMMAND("groups", groups_command, "Print group memberships");
