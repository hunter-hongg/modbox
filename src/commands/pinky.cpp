#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <utmp.h>
#include <argtable3.h>
#include "commands/pinky.hpp"
#include "commands/arg_util.hpp"
#include "commands/utmp_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static std::string format_time(time_t t) {
    char buf[64];
    (void)strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&t));
    return std::string(buf);
}

static std::string format_idle(const struct utmp* u) {
    if (u == nullptr) { return ".";
}
    time_t const now = time(nullptr);
    double const diff = difftime(now, u->ut_time);
    if (diff < 0) { return ".";
}
    long const minutes = static_cast<long>(diff / 60);
    if (minutes < 1) { return ".";
}
    if (minutes < 60) {
        return std::to_string(minutes) + "m";
    } if (minutes < 1440) {
        long const hours = minutes / 60;
        long const mins = minutes % 60;
        if (mins == 0) {
            return std::to_string(hours) + "h";
        }
        return std::to_string(hours) + "h" + std::to_string(mins) + "m";
    } else {
        long const days = minutes / 1440;
        long const rem = minutes % 1440;
        if (rem == 0) {
            return std::to_string(days) + "d";
        }
        long const hours = rem / 60;
        long const mins = rem % 60;
        std::string out = std::to_string(days) + "d";
        if (hours > 0) { out += std::to_string(hours) + "h";
}
        if (mins > 0) { out += std::to_string(mins) + "m";
}
        return out;
    }
}

static std::string get_tty_name(const struct utmp* u) {
    if (strncmp(u->ut_line, "tty", 3) == 0) {
        return std::string("tty") + std::string(u->ut_id);
    }
    return u->ut_line;
}

static int collect_entries(std::vector<struct utmp>& entries) {
    int count = 0;
    for_each_utmp([&entries, &count](const struct utmp& u) {
        if (u.ut_type == USER_PROCESS) {
            entries.push_back(u);
            count++;
        }
    });
    return count;
}

int pinky_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* long_opt = arg_lit0("l", "long", "produce long format output");
    struct arg_lit* brief_opt = arg_lit0("b", "brief", "do not print hostnames");
    struct arg_lit* quick_opt = arg_lit0("q", "quick", "just print the name and count");
    struct arg_lit* no_host_opt = arg_lit0("f", "no-host", "omit remote hostname");
    struct arg_lit* no_full_opt = arg_lit0("i", "no-full-name", "omit user's full name");
    struct arg_lit* no_plan_opt = arg_lit0("p", "no-plan", "omit user's plan file");
    struct arg_lit* short_opt = arg_lit0("s", "short", "short format (like -b)");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, long_opt, brief_opt, quick_opt,
                 no_host_opt, no_full_opt, no_plan_opt, short_opt, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Display user information, or who is logged in.\n");
        printf("\n");
        printf("  -l, --long        produce long format output\n");
        printf("  -b, --brief       do not print hostnames\n");
        printf("  -f, --no-host     omit remote hostname\n");
        printf("  -i, --no-full-name omit user's full name\n");
        printf("  -p, --no-plan     omit user's plan file\n");
        printf("  -s, --short       short format (like -b)\n");
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

    bool const long_format = long_opt->count > 0;
    bool const brief = brief_opt->count > 0 || short_opt->count > 0;
    bool const quick = quick_opt->count > 0;
    bool const show_host = no_host_opt->count == 0;
    (void)no_full_opt;
    (void)no_plan_opt;

    std::vector<struct utmp> entries;
    int const count = collect_entries(entries);

    if (quick) {
        for (const auto& u : entries) {
            printf("%s ", u.ut_user);
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

        for (const auto& u : entries) {
            if (brief) {
                printf("%-8s %-8s ", u.ut_user, get_tty_name(&u).c_str());
                printf("%-12s ", format_idle(&u).c_str());
                printf("%-18s ", format_time(u.ut_time).c_str());
                printf("%s\n", "(none)");
            } else {
                printf("%-8s %-8s ", u.ut_user, get_tty_name(&u).c_str());
                printf("%-12s ", format_idle(&u).c_str());
                printf("%-18s ", format_time(u.ut_time).c_str());
                if (show_host && strlen(u.ut_host) > 0) {
                    printf("%-15s", u.ut_host);
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

        for (const auto& u : entries) {
            printf("%-8s %-8s ", u.ut_user, get_tty_name(&u).c_str());
            printf("%-12s ", format_idle(&u).c_str());
            printf("%-18s ", format_time(u.ut_time).c_str());
            if (show_host && strlen(u.ut_host) > 0) {
                printf(" %s", u.ut_host);
            }
            printf("\n");
        }
    }

    return 0;
}

REGISTER_COMMAND("pinky", pinky_command, "Display user information");
