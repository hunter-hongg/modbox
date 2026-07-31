#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <unistd.h>
#include <argtable3.h>

#include "commands/uptime.hpp"
#include "commands/arg_util.hpp"
#include "commands/utmp_util.hpp"
#include "commands/command_macros.hpp"

int uptime_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* pretty_opt = arg_lit0("p", "pretty", "show uptime in pretty format");
    struct arg_end* end = arg_end(20);
    ArgTable at({help_opt, pretty_opt, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Print system uptime and load averages.\n");
        printf("\n");
        printf("  -p, --pretty  show uptime in pretty format\n");
        printf("  -h, --help    display this help and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    FILE* fp = fopen("/proc/uptime", "r");
    if (!fp) {
        fprintf(stderr, "uptime: cannot open /proc/uptime\n");
        return 0;
    }

    double uptime_secs = 0;
    if (fscanf(fp, "%lf", &uptime_secs) != 1) {
        uptime_secs = 0;
    }
    fclose(fp);

    double loadavg[3] = {0, 0, 0};
    fp = fopen("/proc/loadavg", "r");
    if (fp) {
        if (fscanf(fp, "%lf %lf %lf", &loadavg[0], &loadavg[1], &loadavg[2]) != 3) {
            loadavg[0] = loadavg[1] = loadavg[2] = 0;
        }
        fclose(fp);
    }

    int users = utmp_user_count();

    time_t now = time(nullptr);
    struct tm* tm_now = localtime(&now);
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_now);

    int days = (int)(uptime_secs / 86400);
    int hours = (int)((uptime_secs - days * 86400) / 3600);
    int minutes = (int)((uptime_secs - days * 86400 - hours * 3600) / 60);

    if (pretty_opt->count > 0) {
        if (days > 0) {
            printf("up %d day%s, %d hour%s, %d minute%s\n",
                   days, days == 1 ? "" : "s",
                   hours, hours == 1 ? "" : "s",
                   minutes, minutes == 1 ? "" : "s");
        } else if (hours > 0) {
            printf("up %d hour%s, %d minute%s\n",
                   hours, hours == 1 ? "" : "s",
                   minutes, minutes == 1 ? "" : "s");
        } else {
            printf("up %d minute%s\n",
                   minutes, minutes == 1 ? "" : "s");
        }
    } else {
        printf(" %s up", time_buf);
        if (days > 0) {
            printf(" %d day%s", days, days == 1 ? "" : "s");
        }
        if (hours > 0 || days > 0) {
            printf(" %d:%02d", hours, minutes);
        } else {
            printf(" %d min", minutes);
        }
        printf(", %d user%s", users, users == 1 ? "" : "s");
        printf(", load average: %.2f, %.2f, %.2f\n", loadavg[0], loadavg[1], loadavg[2]);
    }

    return 0;
}

REGISTER_COMMAND("uptime", uptime_command, "Print system uptime and load averages");