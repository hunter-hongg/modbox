#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <vector>

#include "commands/id.hpp"

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
    long v = strtol(name, &end, 10);
    if (*end == '\0' && end != name) {
        *pw = getpwuid((uid_t)v);
        return (uid_t)v;
    }
    *pw = getpwnam(name);
    if (*pw == nullptr) {
        fprintf(stderr, "id: '%s': no such user\n", name);
        exit(1);
    }
    return (*pw)->pw_uid;
}

static gid_t resolve_group(const char* name, struct group** gr) {
    char* end = nullptr;
    long v = strtol(name, &end, 10);
    if (*end == '\0' && end != name) {
        *gr = getgrgid((gid_t)v);
        return (gid_t)v;
    }
    *gr = getgrnam(name);
    if (*gr == nullptr) {
        fprintf(stderr, "id: '%s': no such group\n", name);
        exit(1);
    }
    return (*gr)->gr_gid;
}

void id_command(int argc, char** argv) {
    const char* prog = argv[0];
    id_opts o;
    const char* user_arg = nullptr;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            printf("id (modbox) 1.0\n");
            return;
        }
        if (a[0] == '-' && a[1] != '\0') {
            if (a[1] == '-') {
                if (strcmp(a, "--user") == 0) o.opt_u = true;
                else if (strcmp(a, "--group") == 0) o.opt_g = true;
                else if (strcmp(a, "--groups") == 0) o.opt_G = true;
                else if (strcmp(a, "--name") == 0) o.opt_n = true;
                else if (strcmp(a, "--real") == 0) o.opt_r = true;
                else if (strcmp(a, "--zero") == 0) o.opt_z = true;
                else { fprintf(stderr, "id: unrecognized option '%s'\n", a); return; }
            } else {
                for (const char* p = a + 1; *p; p++) {
                    switch (*p) {
                        case 'u': o.opt_u = true; break;
                        case 'g': o.opt_g = true; break;
                        case 'G': o.opt_G = true; break;
                        case 'n': o.opt_n = true; break;
                        case 'r': o.opt_r = true; break;
                        case 'z': o.opt_z = true; break;
                        default:
                            fprintf(stderr, "id: invalid option -- '%c'\n", *p);
                            return;
                    }
                }
            }
        } else {
            user_arg = a;
        }
    }

    char sep = o.opt_z ? '\0' : ' ';
    char nl = o.opt_z ? '\0' : '\n';

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

    uid_t out_uid = o.opt_r ? ruid : uid;
    gid_t out_gid = o.opt_r ? rgid : gid;

    if (o.opt_u) {
        if (o.opt_n) {
            struct passwd* p = getpwuid(out_uid);
            printf("%s", p ? p->pw_name : "?");
        } else {
            printf("%u", (unsigned)out_uid);
        }
        putchar(nl);
        return;
    }

    if (o.opt_g) {
        if (o.opt_n) {
            struct group* g = getgrgid(out_gid);
            printf("%s", g ? g->gr_name : "?");
        } else {
            printf("%u", (unsigned)out_gid);
        }
        putchar(nl);
        return;
    }

    if (o.opt_G) {
        int ngroups = 0;
        getgroups(0, nullptr);
        std::vector<gid_t> groups(64);
        ngroups = getgroups((int)groups.size(), groups.data());
        bool first = true;
        for (int j = 0; j < ngroups; j++) {
            if (!first) putchar(sep);
            first = false;
            if (o.opt_n) {
                struct group* g = getgrgid(groups[j]);
                printf("%s", g ? g->gr_name : "?");
            } else {
                printf("%u", (unsigned)groups[j]);
            }
        }
        putchar(nl);
        return;
    }

    const char* uname = pw ? pw->pw_name : "?";
    struct group* eg = getgrgid(out_gid);
    const char* gname = eg ? eg->gr_name : "?";

    if (o.opt_n) {
        printf("uid=%u(%s)", (unsigned)out_uid, uname);
    } else {
        printf("uid=%u", (unsigned)out_uid);
    }
    if (out_uid != ruid) {
        struct passwd* rp = getpwuid(ruid);
        printf(" uid=%u(%s)", (unsigned)ruid, rp ? rp->pw_name : "?");
    }

    if (o.opt_n) {
        printf(" gid=%u(%s)", (unsigned)out_gid, gname);
    } else {
        printf(" gid=%u", (unsigned)out_gid);
    }
    if (out_gid != rgid) {
        struct group* rg = getgrgid(rgid);
        printf(" gid=%u(%s)", (unsigned)rgid, rg ? rg->gr_name : "?");
    }

    int ngroups = 0;
    ngroups = getgroups(0, nullptr);
    if (ngroups > 0) {
        std::vector<gid_t> groups(ngroups);
        getgroups(ngroups, groups.data());
        printf(" groups=");
        for (int j = 0; j < ngroups; j++) {
            if (j > 0) putchar(sep);
            if (o.opt_n) {
                struct group* g = getgrgid(groups[j]);
                printf("%u(%s)", (unsigned)groups[j], g ? g->gr_name : "?");
            } else {
                printf("%u", (unsigned)groups[j]);
            }
        }
    }
    putchar(nl);
}
