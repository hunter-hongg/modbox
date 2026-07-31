#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <pwd.h>
#include <utmp.h>
#include <errno.h>
#include <sys/types.h>
#include <argtable3.h>
#include "commands/pinky.hpp"
#include "commands/arg_util.hpp"
#include "commands/utmp_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static std::string format_time(time_t t) {
    char buf[64];
    strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&t));
    return std::string(buf);
}

static std::string format_idle(const struct utmp* u) {
    (void)u;
    return ".";
}

static std::string get_tty_name(const struct utmp* u) {
    if (strncmp(u->ut_line, "tty", 3) == 0) {
        return std::string("tty") + std::string(u->ut_id);
    }
    return u->ut_line;
}

static int collect_entries(std::vector<const struct utmp*>& entries) {
    int count = 0;
    for_each_utmp([&entries, &count](const struct utmp& u) {
        if (u.ut_type == USER_PROCESS) {
            entries.push_back(&u);
            count++;
        }
    });
    return count;
}

static void print_short(const struct utmp* u, bool show_host) {
    printf("%-8s %-8s ", u->ut_user, get_tty_name(u).c_str());
    printf("%s ", format_time(u->ut_time).c_str());
    if (show_host && strlen(u->ut_host) > 0) {
        printf("(%s)", u->ut_host);
    }
    printf("\n");
}

static void print_long(const struct utmp* u, bool show_host) {
    printf("%-8s %-8s ", u->ut_user, get_tty_name(u).c_str());
    printf("%s ", format_time(u->ut_time).c_str());
    if (show_host) {
        if (strlen(u->ut_host) > 0) {
            printf("%-15s ", u->ut_host);
        } else {
            printf("%-15s ", "");
        }
    }
    printf("\n");
}

int pinky_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* long_opt = arg_lit0("l", "long", "produce long format output");
    struct arg_lit* brief_opt = arg_lit0("b", "brief", "do not print hostnames");
    struct arg_lit* quick_opt = arg_lit0("q", "quick", "just print the name and count");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, long_opt, brief_opt, quick_opt, end});
    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Display user information, or who is logged in.\n");
        printf("\n");
        printf("  -l, --long        produce long format output\n");
        printf("  -b, --brief       do not print hostnames\n");
        printf("  -q, --quick       just print the name and count\n");
        printf("  -h, --help        display this help and exit\n");
        printf("\n");
        printf("With no FILE, read /var/run/utmp.\n");
        printf("FILE is a utmp file, typically /var/run/utmp.\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("pinky");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    bool long_format = long_opt->count > 0;
    bool brief = brief_opt->count > 0;
    bool quick = quick_opt->count > 0;

    std::vector<const struct utmp*> entries;
    int count = collect_entries(entries);

    if (quick) {
        for (const auto* u : entries) {
            printf("%s ", u->ut_user);
        }
        printf("total %d\n", count);
        return 0;
    }

    if (long_format || brief) {
        printf("%-8s %-8s ", "Login", "name");
        printf("%-12s ", "TTY");
        printf("%-14s ", "Idle");
        printf("%-18s ", "When");
        if (brief) {
            printf("%s", "Login");
        } else {
            printf("%-15s", "Where");
        }
        printf("\n");

        for (const auto* u : entries) {
            if (brief) {
                printf("%-8s %-8s ", u->ut_user, get_tty_name(u).c_str());
                printf("%-12s ", format_idle(u).c_str());
                printf("%-18s ", format_time(u->ut_time).c_str());
                printf("%s\n", "(none)");
            } else {
                printf("%-8s %-8s ", u->ut_user, get_tty_name(u).c_str());
                printf("%-12s ", format_idle(u).c_str());
                printf("%-18s ", format_time(u->ut_time).c_str());
                if (strlen(u->ut_host) > 0) {
                    printf("%-15s", u->ut_host);
                }
                printf("\n");
            }
        }
    } else {
        printf("%-8s %-8s ", "Login", "name");
        printf("%-12s ", "TTY");
        printf("%-14s ", "Idle");
        printf("%-18s ", "When");
        printf("%s\n", "Login  Where");

        for (const auto* u : entries) {
            printf("%-8s %-8s ", u->ut_user, get_tty_name(u).c_str());
            printf("%-12s ", format_idle(u).c_str());
            printf("%-18s ", format_time(u->ut_time).c_str());
            if (strlen(u->ut_host) > 0) {
                printf(" %s", u->ut_host);
            }
            printf("\n");
        }
    }

    return 0;
}

REGISTER_COMMAND("pinky", pinky_command, "Display user information");
