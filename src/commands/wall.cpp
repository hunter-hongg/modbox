#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <utmp.h>
#include <errno.h>
#include <sys/types.h>
#include <argtable3.h>
#include "commands/wall.hpp"
#include "commands/arg_util.hpp"
#include "commands/utmp_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static char get_hostname_buffer[256];

static const char* get_local_hostname() {
    if (gethostname(get_hostname_buffer, sizeof(get_hostname_buffer)) == 0) {
        return get_hostname_buffer;
    }
    return "localhost";
}

static bool is_root() {
    return geteuid() == 0;
}

static void write_to_tty(const std::string &tty, const std::string &message) {
    int fd = open(tty.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        size_t pos = 0;
        while (pos < message.size()) {
            ssize_t n = write(fd, message.data() + pos, message.size() - pos);
            if (n > 0) pos += n;
            else if (n == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            else break;
        }
        close(fd);
    }
}

int wall_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* nobanner_opt = arg_lit0("n", "nobanner", "do not print banner");
    struct arg_str* group_opt = arg_str0("g", "group", "<GROUP>", "only send to group members");
    struct arg_str* timeout_opt = arg_str0("t", "timeout", "<SEC>", "write timeout in seconds");
    struct arg_str* msg_args = arg_strn(NULL, NULL, "<message>", 0, argc, "message to broadcast");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, nobanner_opt, group_opt, timeout_opt, msg_args, end});
    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: wall [OPTION]... [<file> | <message>]\n");
        printf("Write a message to all logged-in users.\n");
        printf("\n");
        printf("  -g, --group <GROUP>   only send message to group\n");
        printf("  -n, --nobanner        do not print banner (root only)\n");
        printf("  -t, --timeout <SEC>   write timeout in seconds\n");
        printf("  -h, --help            display this help and exit\n");
        printf("  -V, --version         output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("wall");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    if (nobanner_opt->count > 0 && !is_root()) {
        fprintf(stderr, "%s: cannot use --nobanner: permission denied\n", argv[0]);
        return 0;
    }

    const char *group_name = nullptr;
    if (group_opt->count > 0) {
        group_name = group_opt->sval[0];
        struct group *gr = getgrnam(group_name);
        if (gr == nullptr) {
            fprintf(stderr, "%s: unknown group: %s\n", argv[0], group_name);
            return 0;
        }
    }

    std::string message;
    if (msg_args->count > 0) {
        for (int i = 0; i < msg_args->count; i++) {
            if (i > 0) message += " ";
            message += msg_args->sval[i];
        }
    } else {
        char buf[4096];
        while (fgets(buf, sizeof(buf), stdin)) message += buf;
        if (message.empty()) message = "\n";
    }

    std::string full_message = message;
    if (nobanner_opt->count == 0) {
        const char *sender = getlogin() ? getlogin() : "unknown";
        std::string banner = std::string("\nBroadcast message from ") + sender + "@" +
                             get_local_hostname() + " (" + ctime(nullptr) + ")";
        full_message = banner + "\n" + message;
    }

    int sent = 0;
    for_each_utmp([&](const struct utmp& u) {
        if (u.ut_type == USER_PROCESS && strlen(u.ut_line) > 0) {
            if (group_name != nullptr) {
                struct passwd *pw = getpwnam(u.ut_user);
                if (pw == nullptr) return;
                gid_t *gids = nullptr;
                int ngroups = 32;
                gids = (gid_t*)malloc((size_t)ngroups * sizeof(gid_t));
                if (gids == nullptr) return;
                struct group *gr = getgrnam(group_name);
                if (gr == nullptr) { free(gids); return; }
                if (getgrouplist(pw->pw_name, pw->pw_gid, gids, &ngroups) >= 0) {
                    bool member = false;
                    for (int i = 0; i < ngroups; i++) {
                        if (gids[i] == gr->gr_gid) { member = true; break; }
                    }
                    free(gids);
                    if (!member) return;
                } else {
                    free(gids);
                    return;
                }
            }
            std::string tty = "/dev/" + std::string(u.ut_line);
            write_to_tty(tty, full_message);
            sent++;
        }
    });

    return 0;
}

REGISTER_COMMAND("wall", wall_command, "Write a message to all logged-in users");