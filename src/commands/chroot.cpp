#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/fsuid.h>

#include "commands/chroot.hpp"
#include "commands/command_macros.hpp"

static bool parse_userspec(const char* spec, uid_t* uid, gid_t* gid,
                            const char** user_name, const char** group_name) {
    // Format: [user][:group]
    const char* colon = strchr(spec, ':');
    if (!colon) {
        // Just a user name or numeric uid
        char* endptr = nullptr;
        long val = strtol(spec, &endptr, 10);
        if (*endptr == '\0' && val >= 0) {
            *uid = (uid_t)val;
            user_name = nullptr;
        } else {
            struct passwd* pw = getpwnam(spec);
            if (!pw) {
                fprintf(stderr, "chroot: invalid user '%s'\n", spec);
                return false;
            }
            *uid = pw->pw_uid;
            *gid = pw->pw_gid;
        }
        return true;
    }

    // Has colon: user:group or :group or user:
    size_t user_len = colon - spec;
    if (user_len > 0) {
        std::string user_str(spec, user_len);
        char* endptr = nullptr;
        long val = strtol(user_str.c_str(), &endptr, 10);
        if (*endptr == '\0' && val >= 0) {
            *uid = (uid_t)val;
        } else {
            struct passwd* pw = getpwnam(user_str.c_str());
            if (!pw) {
                fprintf(stderr, "chroot: invalid user '%s'\n", user_str.c_str());
                return false;
            }
            *uid = pw->pw_uid;
        }
    }

    const char* group_str = colon + 1;
    if (*group_str) {
        char* endptr = nullptr;
        long val = strtol(group_str, &endptr, 10);
        if (*endptr == '\0' && val >= 0) {
            *gid = (gid_t)val;
        } else {
            struct group* gr = getgrnam(group_str);
            if (!gr) {
                fprintf(stderr, "chroot: invalid group '%s'\n", group_str);
                return false;
            }
            *gid = gr->gr_gid;
        }
    }

    return true;
}

void chroot_command(int argc, char** argv) {
    struct arg_str* userspec_opt = arg_str0(NULL, "userspec", "USER[:GROUP]",
                                             "specify the user and group to use");
    struct arg_str* groups_opt = arg_str0(NULL, "groups", "GROUPS",
                                           "specify supplementary groups as a comma-separated list");
    struct arg_lit* skip_chdir_opt = arg_lit0(NULL, "skip-chdir",
                                               "do not change working directory to root");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* newroot_arg = arg_filen(NULL, NULL, "NEWROOT", 1, 1, "new root directory");
    struct arg_file* command_arg = arg_filen(NULL, NULL, "COMMAND", 0, 1, "command to run (default /bin/sh)");
    struct arg_str* cmd_args = arg_strn(NULL, NULL, "ARG", 0, 100, "command arguments");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {userspec_opt, groups_opt, skip_chdir_opt,
                        help_opt, newroot_arg, command_arg, cmd_args, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION] NEWROOT [COMMAND [ARG]...]\n", argv[0]);
        printf("  or:  %s [OPTION]\n", argv[0]);
        printf("Run COMMAND with root directory set to NEWROOT.\n");
        printf("\n");
        printf("      --userspec=USER[:GROUP]  specify user and group (ID or name) to use\n");
        printf("      --groups=G_LIST          specify supplementary groups as a comma-separated list\n");
        printf("      --skip-chdir             do not change working directory to root\n");
        printf("      --help                   display this help and exit\n");
        printf("\n");
        printf("If no command is given, run /bin/sh.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    const char* newroot = newroot_arg->filename[0];

    // Determine command and args
    const char* cmd = "/bin/sh";
    std::vector<const char*> exec_argv;
    exec_argv.push_back(cmd);

    if (command_arg->count > 0) {
        cmd = command_arg->filename[0];
        exec_argv[0] = cmd;
    }

    for (int i = 0; i < cmd_args->count; i++) {
        exec_argv.push_back(cmd_args->sval[i]);
    }
    exec_argv.push_back(nullptr);

    // chroot requires root privileges
    if (geteuid() != 0) {
        fprintf(stderr, "chroot: must be run as root\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Step 1: Set supplementary groups (before chroot)
    if (groups_opt->count > 0) {
        // Parse comma-separated group list
        std::string groups_str(groups_opt->sval[0]);
        std::vector<gid_t> gid_list;
        size_t start = 0;
        while (start < groups_str.length()) {
            size_t end = groups_str.find(',', start);
            if (end == std::string::npos) end = groups_str.length();
            std::string token = groups_str.substr(start, end - start);
            char* endptr = nullptr;
            long val = strtol(token.c_str(), &endptr, 10);
            if (*endptr == '\0' && val >= 0) {
                gid_list.push_back((gid_t)val);
            } else {
                struct group* gr = getgrnam(token.c_str());
                if (!gr) {
                    fprintf(stderr, "chroot: invalid group '%s'\n", token.c_str());
                    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
                    return;
                }
                gid_list.push_back(gr->gr_gid);
            }
            start = end + 1;
        }
        if (setgroups(gid_list.size(), gid_list.data()) != 0) {
            fprintf(stderr, "chroot: failed to set groups: %s\n", strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    // Step 2: chroot()
    if (chroot(newroot) != 0) {
        fprintf(stderr, "chroot: failed to change root directory to '%s': %s\n",
                newroot, strerror(errno));
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Step 3: cd to root (unless --skip-chdir)
    if (skip_chdir_opt->count == 0) {
        if (chdir("/") != 0) {
            fprintf(stderr, "chroot: failed to change working directory: %s\n",
                    strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    // Step 4: Set user/group (--userspec)
    if (userspec_opt->count > 0) {
        uid_t uid = 0;
        gid_t gid = 0;
        if (!parse_userspec(userspec_opt->sval[0], &uid, &gid, nullptr, nullptr)) {
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        // Set GID first, then UID (permissions check)
        if (setgid(gid) != 0) {
            fprintf(stderr, "chroot: failed to set group ID: %s\n", strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        if (setuid(uid) != 0) {
            fprintf(stderr, "chroot: failed to set user ID: %s\n", strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    // Step 5: exec the command
    execvp(cmd, (char* const*)exec_argv.data());

    // If exec fails
    fprintf(stderr, "chroot: failed to execute '%s': %s\n", cmd, strerror(errno));
    exit(127);
}

REGISTER_COMMAND("chroot", chroot_command, "Change root directory and run command");
