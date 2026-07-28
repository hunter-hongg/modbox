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
#include "commands/who.hpp"
#include "commands/command_macros.hpp"

static char get_hostname_buffer[256];

static const char* get_local_hostname() {
    if (gethostname(get_hostname_buffer, sizeof(get_hostname_buffer)) == 0) {
        return get_hostname_buffer;
    }
    return "localhost";
}

static void print_heading() {
    printf("%-8s %-6s %-10s %-15s\n", "NAME", "LINE", "FROM", "TIME");
}

static std::string format_time(time_t t) {
    char buf[64];
    strftime(buf, sizeof(buf), "%a %b %e %H:%M", localtime(&t));
    return std::string(buf);
}

void who_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_str* utmp_file = arg_str0(NULL, NULL, "<FILE>", "use FILE instead of /var/run/utmp");
    struct arg_lit* a_opt = arg_lit0("a", "all", "print everything available");
    struct arg_lit* b_opt = arg_lit0("b", "boot", "time of last system boot");
    struct arg_lit* d_opt = arg_lit0("d", "dead", "entries of dead processes");
    struct arg_lit* H_opt = arg_lit0("H", "heading", "print column names");
    struct arg_lit* l_opt = arg_lit0("l", "login", "line number of system login process");
    struct arg_lit* p_opt = arg_lit0("p", "process", "entries spawned by init");
    struct arg_lit* q_opt = arg_lit0("q", "count", "all login names and a count of users");
    struct arg_lit* r_opt = arg_lit0("r", "runlevel", "current runlevel");
    struct arg_lit* s_opt = arg_lit0("s", "short", "short listing");
    struct arg_lit* t_opt = arg_lit0("t", "time", "time of last system clock change");
    struct arg_lit* T_opt = arg_lit0("T", "mesg", "write message status as terminal type");
    struct arg_lit* u_opt = arg_lit0("u", "users", "list users logged in including idle time");
    struct arg_lit* m_opt = arg_lit0("m", NULL, "only user of current line");

    struct arg_end* end = arg_end(20);

    void* argtable[] = {help_opt, version_opt, utmp_file, a_opt, b_opt, d_opt, H_opt, l_opt, p_opt, q_opt, r_opt, s_opt, t_opt, T_opt, u_opt, m_opt, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: who [OPTION]... [FILE] [am i]\n");
        printf("Show who is logged in.\n");
        printf("\n");
        printf("  -a, --all           equivalent to -b -d -l -p -r -t -T -u\n");
        printf("  -b, --boot          time of last system boot\n");
        printf("  -d, --dead          entries of dead processes\n");
        printf("  -H, --heading       print column names\n");
        printf("  -l, --login         lines of system login process\n");
        printf("  -p, --process       processes spawned by init\n");
        printf("  -q, --count         quick list of login names and count\n");
        printf("  -r, --runlevel      current runlevel\n");
        printf("  -s, --short         short listing (default)\n");
        printf("  -t, --time          time of last system clock change\n");
        printf("  -T, -w, --mesg      same as -u, write allowed status shown\n");
        printf("  -u, --users         list users logged in with idle time\n");
        printf("  -m                  only user whose stdin is connected\n");
        printf("  --help              display this help and exit\n");
        printf("  --version           output version information and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (version_opt->count > 0) {
        printf("who (modbox) 1.0\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool show_all = a_opt->count > 0;
    bool show_boot = b_opt->count > 0 || show_all;
    bool show_dead = d_opt->count > 0 || show_all;
    bool show_login = l_opt->count > 0 || show_all;
    bool show_process = p_opt->count > 0 || show_all;
    bool show_count = q_opt->count > 0 || show_all;
    bool show_runlevel = r_opt->count > 0 || show_all;
    bool show_time = t_opt->count > 0 || show_all;
    bool show_message = T_opt->count > 0 || show_all;
    bool show_users = u_opt->count > 0 || show_all;
    bool show_short = s_opt->count > 0 || (!show_all && !show_count && !show_users);
    bool heading = H_opt->count > 0;

    if (!show_all && !show_boot && !show_dead && !show_login && !show_process &&
        !show_count && !show_runlevel && !show_time && !show_message && !show_users) {
        show_short = true;
    }

    if (show_count) {
        setutent();
        struct utmp *u;
        int count = 0;
        while ((u = getutent()) != NULL) {
            if (u->ut_type == USER_PROCESS && strlen(u->ut_line) > 0) count++;
        }
        endutent();
        printf("total %d\n", count);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    setutent();
    struct utmp *u;
    std::vector<struct utmp*> entries;

    while ((u = getutent()) != NULL) {
        entries.push_back(u);
    }
    endutent();

    if (heading) print_heading();

    for (const auto& entry : entries) {
        if (entry->ut_type == DEAD_PROCESS) {
            if (show_dead) {
                printf("%-8s %-6s %s\n", entry->ut_user, entry->ut_line, "(still dead)");
            }
        } else if (entry->ut_type == BOOT_TIME) {
            if (show_boot) {
                printf("system boot  %s %s\n", entry->ut_line, format_time(entry->ut_time).c_str());
            }
        } else if (entry->ut_type == RUN_LVL) {
            if (show_runlevel) {
                printf("run-level  %s %s %s\n", entry->ut_line, format_time(entry->ut_time).c_str(), entry->ut_id);
            }
        } else if (entry->ut_type == LOGIN_PROCESS) {
            if (show_login) {
                printf("%-8s %-6s ", entry->ut_user, entry->ut_line);
                if (strlen(entry->ut_host) > 0) printf("(%s)", entry->ut_host);
                printf("\n");
            }
        } else if (entry->ut_type == INIT_PROCESS) {
            if (show_process) {
                printf("%-8s %-6s %s\n", entry->ut_user, entry->ut_line, entry->ut_host);
            }
        } else if (entry->ut_type == NEW_TIME) {
            if (show_time) {
                printf("clock change  %s\n", format_time(entry->ut_time).c_str());
            }
        } else if (entry->ut_type == OLD_TIME) {
            if (show_time) {
                printf("clock change (old)  %s\n", format_time(entry->ut_time).c_str());
            }
        } else if (entry->ut_type == USER_PROCESS) {
            if (show_short) {
                printf("%-8s %-6s ", entry->ut_user, entry->ut_line);
                if (strlen(entry->ut_host) > 0) printf("(%s)", entry->ut_host);
                printf(" %-14s\n", format_time(entry->ut_time).c_str());
            }
            if (show_users) {
                printf("%-8s %-6s %-10s %-15s %-8s %-8s %-8s\n",
                       entry->ut_user, entry->ut_line,
                       strlen(entry->ut_host) > 0 ? entry->ut_host : "",
                       format_time(entry->ut_time).c_str(), "?", "?", "?");
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("who", who_command, "Show who is logged in");