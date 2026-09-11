#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <vector>

#include "commands/id.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [USER]\n", prog);
    printf("Print user and group information for the specified USER,\n");
    printf("or (when USER omitted) for the current user.\n");
    printf("\n");
    printf("  -u, --user     print only the effective user ID\n");
    printf("  -g, --group    print only the effective group ID\n");
    printf("  -G, --groups   print all group IDs\n");
    printf("  -n, --name     print a name instead of a number\n");
    printf("  -r, --real     print the real ID instead of the effective ID\n");
    printf("  -z, --zero     delimit output with NUL, not whitespace\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -V, --version  output version information and exit\n");
}

struct id_opts {
    bool opt_u = false;
    bool opt_g = false;
    bool opt_G = false;
    bool opt_n = false;
    bool opt_r = false;
    bool opt_z = false;
};

static uid_t resolve_user(const char* name, struct passwd** pw) {
    char* end = nullptr;
    long const v = strtol(name, &end, 10);
    if (*end == '\0' && end != name) {
        *pw = getpwuid(static_cast<uid_t>(v));
        return static_cast<uid_t>(v);
    }
    *pw = getpwnam(name);
    if (*pw == nullptr) {
        (void)fprintf(stderr, "id: '%s': no such user\n", name);
        exit(1);
    }
    return (*pw)->pw_uid;
}

static gid_t resolve_group(const char* name, struct group** gr) {
    char* end = nullptr;
    long const v = strtol(name, &end, 10);
    if (*end == '\0' && end != name) {
        *gr = getgrgid(static_cast<gid_t>(v));
        return static_cast<gid_t>(v);
    }
    *gr = getgrnam(name);
    if (*gr == nullptr) {
        (void)fprintf(stderr, "id: '%s': no such group\n", name);
        exit(1);
    }
    return (*gr)->gr_gid;
}

int id_command(int argc, char** argv) {
    const char* prog = argv[0];
    id_opts o;
    const char* user_arg = nullptr;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_help(prog);
            return 0;
        }
        if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            print_version("id");
            return 0;
        }
        if (a[0] == '-' && a[1] != '\0') {
            if (a[1] == '-') {
                if (strcmp(a, "--user") == 0) { { o.opt_u = true;
                } } else if (strcmp(a, "--group") == 0) { { o.opt_g = true;
                } } else if (strcmp(a, "--groups") == 0) { { o.opt_G = true;
                } } else if (strcmp(a, "--name") == 0) { { o.opt_n = true;
                } } else if (strcmp(a, "--real") == 0) { { o.opt_r = true;
                } } else if (strcmp(a, "--zero") == 0) { { o.opt_z = true;
                } } else { (void)fprintf(stderr, "id: unrecognized option '%s'\n", a); return 0; }
            } else {
                for (const char* p = a + 1; (*p) != 0; p++) {
                    switch (*p) {
                        case 'u': o.opt_u = true; break;
                        case 'g': o.opt_g = true; break;
                        case 'G': o.opt_G = true; break;
                        case 'n': o.opt_n = true; break;
                        case 'r': o.opt_r = true; break;
                        case 'z': o.opt_z = true; break;
                        default:
                            (void)fprintf(stderr, "id: invalid option -- '%c'\n", *p);
                            return 0;
                    }
                }
            }
        } else {
            user_arg = a;
        }
    }

    char const sep = o.opt_z ? '\0' : ' ';
    char const nl = o.opt_z ? '\0' : '\n';

    struct passwd* pw = nullptr;
    uid_t uid;
    gid_t gid;
    uid_t ruid;
    gid_t rgid;

    if (user_arg != nullptr) {
        uid = resolve_user(user_arg, &pw);
        gid = pw->pw_gid;
        ruid = uid;
        rgid = gid;
    } else {
        uid = geteuid();
        gid = getegid();
        ruid = getuid();
        rgid = getgid();
        pw = getpwuid(uid);
    }

    uid_t const out_uid = o.opt_r ? ruid : uid;
    gid_t const out_gid = o.opt_r ? rgid : gid;

    if (o.opt_u) {
        if (o.opt_n) {
            const struct passwd* p = getpwuid(out_uid);
            printf("%s", (p != nullptr) ? p->pw_name : "?");
        } else {
            printf("%u", static_cast<unsigned>(out_uid));
        }
        putchar(nl);
        return 0;
    }

    if (o.opt_g) {
        if (o.opt_n) {
            const struct group* g = getgrgid(out_gid);
            printf("%s", (g != nullptr) ? g->gr_name : "?");
        } else {
            printf("%u", static_cast<unsigned>(out_gid));
        }
        putchar(nl);
        return 0;
    }

    if (o.opt_G) {
        int ngroups = 0;
        getgroups(0, nullptr);
        std::vector<gid_t> groups(64);
        ngroups = getgroups(static_cast<int>(groups.size()), groups.data());
        bool first = true;
        for (int j = 0; j < ngroups; j++) {
            if (!first) { putchar(sep);
}
            first = false;
            if (o.opt_n) {
                const struct group* g = getgrgid(groups[j]);
                printf("%s", (g != nullptr) ? g->gr_name : "?");
            } else {
                printf("%u", static_cast<unsigned>(groups[j]));
            }
        }
        putchar(nl);
        return 0;
    }

    const char* uname = (pw != nullptr) ? pw->pw_name : "?";
    const struct group* eg = getgrgid(out_gid);
    const char* gname = (eg != nullptr) ? eg->gr_name : "?";

    if (o.opt_n) {
        printf("uid=%u(%s)", static_cast<unsigned>(out_uid), uname);
    } else {
        printf("uid=%u", static_cast<unsigned>(out_uid));
    }
    if (out_uid != ruid) {
        const struct passwd* rp = getpwuid(ruid);
        printf(" uid=%u(%s)", static_cast<unsigned>(ruid), (rp != nullptr) ? rp->pw_name : "?");
    }

    if (o.opt_n) {
        printf(" gid=%u(%s)", static_cast<unsigned>(out_gid), gname);
    } else {
        printf(" gid=%u", static_cast<unsigned>(out_gid));
    }
    if (out_gid != rgid) {
        const struct group* rg = getgrgid(rgid);
        printf(" gid=%u(%s)", static_cast<unsigned>(rgid), (rg != nullptr) ? rg->gr_name : "?");
    }

    int ngroups = 0;
    ngroups = getgroups(0, nullptr);
    if (ngroups > 0) {
        std::vector<gid_t> groups(ngroups);
        getgroups(ngroups, groups.data());
        printf(" groups=");
        for (int j = 0; j < ngroups; j++) {
            if (j > 0) { putchar(sep);
}
            if (o.opt_n) {
                const struct group* g = getgrgid(groups[j]);
                printf("%u(%s)", static_cast<unsigned>(groups[j]), (g != nullptr) ? g->gr_name : "?");
            } else {
                printf("%u", static_cast<unsigned>(groups[j]));
            }
        }
    }
    putchar(nl);
    return 0;
}

REGISTER_COMMAND("id", id_command, "Print user identity");
