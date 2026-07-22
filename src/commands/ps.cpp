#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <argtable3.h>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include "commands/tui_base.hpp"
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>

#include "commands/ps.hpp"
#include "commands/command_macros.hpp"

struct ProcessInfo {
    pid_t pid;
    pid_t ppid;
    pid_t pgrp;
    pid_t session;
    uid_t uid;
    gid_t gid;
    char comm[256];
    char state;
    long tty_nr;
    int utime;
    int stime;
    int cutime;
    int cstime;
    int priority;
    int nice;
    int num_threads;
    unsigned long long starttime;
    unsigned long long vsize;
    long long rss;
    char cmd[4096];
};

// Read system boot time (seconds since epoch) from /proc/stat "btime".
static long g_btime = 0;
static int g_clk_tck = 100; // sysconf(_SC_CLK_TCK), updated below

static void read_boot_time() {
    FILE* fp = fopen("/proc/stat", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "btime", 5) == 0) {
                long bt;
                if (sscanf(line, "btime %ld", &bt) == 1) {
                    g_btime = bt;
                }
                break;
            }
        }
        fclose(fp);
    }
}

// Convert a tty device number into a human-readable terminal name.
// tty_nr is major*256 + minor (Linux convention). 0 means no terminal.
static void tty_name(long tty_nr, char* out, size_t out_size) {
    if (tty_nr == 0) {
        snprintf(out, out_size, "?");
        return;
    }
    int major = (tty_nr >> 8) & 0xfff;
    int minor = (tty_nr & 0xff) | ((tty_nr >> 12) & 0xfff00);

    if (major == 4) {
        // 4,x -> ttyx
        snprintf(out, out_size, "tty%d", minor);
    } else if (major == 136 || major == 128) {
        // 136,x / 128,x -> pts/x
        snprintf(out, out_size, "pts/%d", minor);
    } else if (major == 3) {
        // 3,x -> ttySx (serial)
        snprintf(out, out_size, "ttyS%d", minor);
    } else if (major == 348) {
        snprintf(out, out_size, "pts/%d", minor);
    } else {
        snprintf(out, out_size, "%d/%d", major, minor);
    }
}

static bool read_process_info(pid_t pid, ProcessInfo& info) {
    memset(&info, 0, sizeof(info));
    char path[256];
    FILE* fp;

    // Read stat
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) return false;

    char stat_buf[4096];
    if (fgets(stat_buf, sizeof(stat_buf), fp)) {
        // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags
        //   minflt cminflt majflt cmajflt utime stime cutime cstime
        //   priority nice num_threads itrealvalue starttime vsize rss ...
        char* p = stat_buf;

        // Skip pid
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;

        // Read comm (in parentheses)
        if (*p == '(') {
            p++;
            char* comm_start = p;
            while (*p && *p != ')') p++;
            size_t comm_len = p - comm_start;
            if (comm_len < sizeof(info.comm) - 1) {
                memcpy(info.comm, comm_start, comm_len);
                info.comm[comm_len] = '\0';
            }
            p++; // skip ')'
        }

        // Skip whitespace before state
        while (*p == ' ') p++;
        info.state = *p;

        // Parse the remaining fields.
        // Fields after state: ppid pgrp session tty_nr tpgid flags
        //   minflt cminflt majflt cmajflt utime stime cutime cstime
        //   priority nice num_threads itrealvalue starttime vsize rss
        int parsed = sscanf(p,
            "%*c %d %d %d %ld %*d %*d %*d %*d %*d %d %d %d %d %d %d %d %llu %llu %lld",
            &info.ppid, &info.pgrp, &info.session, &info.tty_nr,
            &info.utime, &info.stime, &info.cutime, &info.cstime,
            &info.priority, &info.nice, &info.num_threads,
            &info.starttime, &info.vsize, &info.rss);

        if (parsed < 14) {
            fclose(fp);
            return false;
        }
    }
    fclose(fp);

    // Read status for uid/gid
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                // Format: Uid: real effective saved fs
                int r, e, s, f;
                if (sscanf(line, "Uid: %d %d %d %d", &r, &e, &s, &f) >= 1) {
                    info.uid = e; // effective uid
                }
            } else if (strncmp(line, "Gid:", 4) == 0) {
                int r, e, s, f;
                if (sscanf(line, "Gid: %d %d %d %d", &r, &e, &s, &f) >= 1) {
                    info.gid = e;
                }
            }
        }
        fclose(fp);
    }

    // Read cmdline
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    fp = fopen(path, "r");
    if (fp) {
        size_t n = fread(info.cmd, 1, sizeof(info.cmd) - 1, fp);
        info.cmd[n > 0 ? n - 1 : 0] = '\0'; // ensure terminator
        // Replace null bytes with spaces (args separated by NUL in cmdline)
        for (size_t i = 0; i < n; i++) {
            if (info.cmd[i] == '\0') info.cmd[i] = ' ';
        }
        fclose(fp);
        if (n == 0) {
            // No cmdline (e.g. kernel thread) -> fall back to comm
            strncpy(info.cmd, info.comm, sizeof(info.cmd) - 1);
            info.cmd[sizeof(info.cmd) - 1] = '\0';
        }
    } else {
        strncpy(info.cmd, info.comm, sizeof(info.cmd) - 1);
        info.cmd[sizeof(info.cmd) - 1] = '\0';
    }

    info.pid = pid;
    return true;
}

static const char* get_username(uid_t uid) {
    static char username[256];
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        strncpy(username, pw->pw_name, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    } else {
        snprintf(username, sizeof(username), "%d", uid);
    }
    return username;
}

// Format the process start time as HH:MM:SS (or MM-DD if old).
static void format_start_time(unsigned long long starttime, char* out, size_t out_size) {
    if (g_btime == 0) {
        snprintf(out, out_size, "?");
        return;
    }
    time_t start_sec = (time_t)(g_btime + (long long)(starttime / g_clk_tck));
    time_t now = time(nullptr);
    struct tm tm_start;
    if (localtime_r(&start_sec, &tm_start) == nullptr) {
        snprintf(out, out_size, "?");
        return;
    }
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    if (tm_start.tm_year == tm_now.tm_year && tm_start.tm_mon == tm_now.tm_mon &&
        tm_start.tm_mday == tm_now.tm_mday) {
        // Same day: HH:MM:SS
        snprintf(out, out_size, "%02d:%02d:%02d",
                 tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec);
    } else {
        // Different day: MM-DD
        snprintf(out, out_size, "%02d-%02d",
                 tm_start.tm_mon + 1, tm_start.tm_mday);
    }
}

// Format cumulative CPU time as [[dd-]hh:]mm:ss
static void format_cputime(int total_ticks, char* out, size_t out_size) {
    long total_sec = total_ticks / g_clk_tck;
    long days = total_sec / 86400;
    long hours = (total_sec % 86400) / 3600;
    long mins = (total_sec % 3600) / 60;
    long secs = total_sec % 60;
    if (days > 0) {
        snprintf(out, out_size, "%ld-%02ld:%02ld:%02ld", days, hours, mins, secs);
    } else if (hours > 0) {
        snprintf(out, out_size, "%02ld:%02ld:%02ld", hours, mins, secs);
    } else {
        snprintf(out, out_size, "%02ld:%02ld", mins, secs);
    }
}

// Forward declaration for TUI mode
static void ps_tui_main();

void ps_command(int argc, char** argv) {
    g_clk_tck = (int)sysconf(_SC_CLK_TCK);
    if (g_clk_tck <= 0) g_clk_tck = 100;
    read_boot_time();

    // Pre-process BSD-style option clusters: "aux" -> "-aux", "ax" -> "-ax", etc.
    // This allows both "ps aux" and "ps -aux" to work identically.
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            const char* arg = argv[i];
            size_t len = strlen(arg);
            if (len >= 1 && len <= 5) {
                bool all_alpha = true;
                for (size_t j = 0; j < len; j++) {
                    if (!isalpha((unsigned char)arg[j])) {
                        all_alpha = false;
                        break;
                    }
                }
                if (all_alpha) {
                    std::string dashed = "-";
                    dashed += arg;
                    argv[i] = strdup(dashed.c_str());
                }
            }
        }
    }

    struct arg_lit* all_opt = arg_lit0("A", "all", "select all processes");
    struct arg_lit* a_opt = arg_lit0("a", NULL, "select all processes except session leaders and processes without a terminal");
    struct arg_lit* d_opt = arg_lit0("d", NULL, "select all processes except session leaders");
    struct arg_lit* e_opt = arg_lit0("e", NULL, "select all processes (same as -A)");
    struct arg_lit* f_opt = arg_lit0("f", NULL, "full format listing");
    struct arg_lit* u_opt = arg_lit0("u", NULL, "user-oriented format (BSD style)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* x_opt = arg_lit0("x", NULL, "lift the BSD-style 'must have a tty' restriction");
    struct arg_lit* tui_opt = arg_lit0(NULL, "tui", "interactive TUI mode (procs-style)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {all_opt, a_opt, d_opt, e_opt, f_opt, u_opt, help_opt, x_opt, tui_opt, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Report a snapshot of the current processes.\n");
        printf("\n");
        printf("  -A, --all               select all processes\n");
        printf("  -a                      select all processes except session leaders and\n");
        printf("                          processes without a terminal\n");
        printf("  -d                      select all processes except session leaders\n");
        printf("  -e                      select all processes (same as -A)\n");
        printf("  -f                      full format listing\n");
        printf("  -u                      user-oriented format (BSD style)\n");
        printf("  -h, --help              display this help and exit\n");
        printf("  -x                      lift the BSD-style 'must have a tty' restriction\n");
        printf("      --tui               interactive TUI mode (procs-style)\n");
        printf("\n");
        printf("TUI mode keys:\n");
        printf("  q         Quit\n");
        printf("  j/k       Scroll down/up\n");
        printf("  c/m/p     Sort by CPU/MEM/PID\n");
        printf("  t         Toggle tree view\n");
        printf("  /         Search/filter\n");
        printf("  Esc       Clear filter\n");
        printf("\n");
        printf("By default, ps selects all processes with the same effective user ID\n");
        printf("(EUID) as the current user and associated with the same terminal as the\n");
        printf("invoker.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // TUI mode
    if (tui_opt->count > 0) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        ps_tui_main();
        return;
    }

    bool select_all = (all_opt->count > 0) || (e_opt->count > 0);
    bool select_a = (a_opt->count > 0);
    bool select_d = (d_opt->count > 0);
    bool full_format = (f_opt->count > 0);
    bool select_x = (x_opt->count > 0);
    bool user_format = (u_opt->count > 0);

    // BSD-style: -a -x together = show all processes
    if (select_a && select_x) {
        select_all = true;
    }

    uid_t my_uid = geteuid();
    pid_t my_pid = getpid();

    // Determine our own session/tty for default and -a selection.
    ProcessInfo my_info;
    bool have_my_info = read_process_info(my_pid, my_info);

    // Helper to test whether a process is a session leader.
    auto is_session_leader = [](const ProcessInfo& info) {
        return info.session == info.pid;
    };

    std::vector<ProcessInfo> matched_procs;

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("ps: cannot open /proc");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type != DT_DIR) continue;

        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        ProcessInfo info;
        if (!read_process_info(pid, info)) continue;

        bool show = false;

        if (select_all) {
            show = true;
        } else {
            if (select_a && !is_session_leader(info) && info.tty_nr != 0) {
                show = true;
            }
            if (select_d && !is_session_leader(info)) {
                show = true;
            }
            if (select_x && info.uid == my_uid) {
                show = true;
            }
            if (!select_a && !select_d && !select_x) {
                if (info.uid == my_uid && have_my_info &&
                    info.tty_nr == my_info.tty_nr && info.tty_nr != 0) {
                    show = true;
                } else if (info.uid == my_uid && !have_my_info) {
                    show = true;
                }
            }
        }

        if (!show) continue;
        matched_procs.push_back(info);
    }

    closedir(proc_dir);

    int pid_w = 3, ppid_w = 3, cpu_w = 1, user_w = 3;
    int stime_w = 5, tty_w = 3, time_w = 4;
    int vsz_w = 4, rss_w = 4, stat_w = 1, mem_w = 4;

    // Read total memory for %MEM calculation (user format)
    unsigned long mem_total_kb = 0;
    {
        FILE* f = fopen("/proc/meminfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (sscanf(line, "MemTotal: %lu kB", &mem_total_kb) == 1) break;
            }
            fclose(f);
        }
    }

    for (const auto& info : matched_procs) {
        int n;
        n = snprintf(nullptr, 0, "%d", info.pid);
        if (n > pid_w) pid_w = n;
        n = snprintf(nullptr, 0, "%d", info.ppid);
        if (n > ppid_w) ppid_w = n;
        const char* username = get_username(info.uid);
        n = (int)strlen(username);
        if (n > user_w) user_w = n;

        char stime_str[32];
        format_start_time(info.starttime, stime_str, sizeof(stime_str));
        n = (int)strlen(stime_str);
        if (n > stime_w) stime_w = n;

        char tty_str[64];
        tty_name(info.tty_nr, tty_str, sizeof(tty_str));
        n = (int)strlen(tty_str);
        if (n > tty_w) tty_w = n;

        int total_ticks = info.utime + info.stime + info.cutime + info.cstime;
        char time_str[32];
        format_cputime(total_ticks, time_str, sizeof(time_str));
        n = (int)strlen(time_str);
        if (n > time_w) time_w = n;

        int cpu_pct = 0;
        if (g_btime != 0) {
            long long start_sec = g_btime + (long long)(info.starttime / g_clk_tck);
            long long elapsed = (long long)time(nullptr) - start_sec;
            if (elapsed > 0) {
                cpu_pct = (int)((total_ticks * 100LL / g_clk_tck) / elapsed);
                if (cpu_pct > 99) cpu_pct = 99;
            }
        }
        n = snprintf(nullptr, 0, "%d", cpu_pct);
        if (n > cpu_w) cpu_w = n;

        // User-format column widths
        if (user_format) {
            // %MEM width (XX.Y format)
            n = snprintf(nullptr, 0, "%.1f", 0.0);
            if (n > mem_w) mem_w = n;

            // VSZ width (in KB)
            n = snprintf(nullptr, 0, "%lu", (unsigned long)(info.vsize / 1024));
            if (n > vsz_w) vsz_w = n;

            // RSS width (in KB: rss * pagesize / 1024)
            long rss_kb = info.rss * (long)sysconf(_SC_PAGE_SIZE) / 1024;
            n = snprintf(nullptr, 0, "%ld", rss_kb);
            if (n > rss_w) rss_w = n;
        }
    }

    if (full_format) {
        printf("%-*s %*s %*s %*s %-*s %-*s %-*s %s\n",
               user_w, "UID", pid_w, "PID", ppid_w, "PPID", cpu_w, "C",
               stime_w, "STIME", tty_w, "TTY", time_w, "TIME", "CMD");
    } else if (user_format) {
        printf("%-*s %*s %*s %*s %*s %*s %-*s %-*s %-*s %-*s %s\n",
               user_w, "USER", pid_w, "PID", cpu_w, "%CPU",
               mem_w, "%MEM", vsz_w, "VSZ", rss_w, "RSS",
               tty_w, "TTY", stat_w, "STAT", stime_w, "START",
               time_w, "TIME", "COMMAND");
    } else {
        printf("%*s %-*s %-*s %s\n",
               pid_w, "PID", tty_w, "TTY", time_w, "TIME", "CMD");
    }

    for (const auto& info : matched_procs) {
        char tty_str[64];
        tty_name(info.tty_nr, tty_str, sizeof(tty_str));

        if (full_format) {
            const char* username = get_username(info.uid);

            int total_ticks = info.utime + info.stime + info.cutime + info.cstime;
            int cpu_percent = 0;
            if (g_btime != 0) {
                long long start_sec = g_btime + (long long)(info.starttime / g_clk_tck);
                long long elapsed = (long long)time(nullptr) - start_sec;
                if (elapsed > 0) {
                    cpu_percent = (int)((total_ticks * 100LL / g_clk_tck) / elapsed);
                    if (cpu_percent > 99) cpu_percent = 99;
                }
            }

            char stime_str[32];
            format_start_time(info.starttime, stime_str, sizeof(stime_str));

            char time_str[32];
            format_cputime(total_ticks, time_str, sizeof(time_str));

            printf("%-*s %*d %*d %*d %-*s %-*s %-*s %s\n",
                   user_w, username, pid_w, info.pid, ppid_w, info.ppid,
                   cpu_w, cpu_percent, stime_w, stime_str, tty_w, tty_str,
                   time_w, time_str, info.cmd);
        } else if (user_format) {
            const char* username = get_username(info.uid);

            int total_ticks = info.utime + info.stime + info.cutime + info.cstime;
            int cpu_percent = 0;
            if (g_btime != 0) {
                long long start_sec = g_btime + (long long)(info.starttime / g_clk_tck);
                long long elapsed = (long long)time(nullptr) - start_sec;
                if (elapsed > 0) {
                    cpu_percent = (int)((total_ticks * 100LL / g_clk_tck) / elapsed);
                    if (cpu_percent > 99) cpu_percent = 99;
                }
            }

            double mem_pct = 0.0;
            if (mem_total_kb > 0) {
                long rss_kb = info.rss * (long)sysconf(_SC_PAGE_SIZE) / 1024;
                mem_pct = 100.0 * (double)rss_kb / (double)mem_total_kb;
            }

            unsigned long vsz_kb = (unsigned long)(info.vsize / 1024);
            long rss_kb = info.rss * (long)sysconf(_SC_PAGE_SIZE) / 1024;

            char stime_str[32];
            format_start_time(info.starttime, stime_str, sizeof(stime_str));

            char time_str[32];
            format_cputime(total_ticks, time_str, sizeof(time_str));

            printf("%-*s %*d %*d %*.1f %*lu %*ld %-*s %-*c %-*s %-*s %s\n",
                   user_w, username, pid_w, info.pid,
                   cpu_w, cpu_percent, mem_w, mem_pct,
                   vsz_w, vsz_kb, rss_w, rss_kb,
                   tty_w, tty_str, stat_w, info.state,
                   stime_w, stime_str, time_w, time_str, info.cmd);
        } else {
            int total_ticks = info.utime + info.stime + info.cutime + info.cstime;
            char time_str[32];
            format_cputime(total_ticks, time_str, sizeof(time_str));

            printf("%*d %-*s %-*s %s\n",
                   pid_w, info.pid, tty_w, tty_str, time_w, time_str, info.cmd);
        }
    }
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

// ---------------------------------------------------------------------------
// TUI mode implementation (procs-style)
// ---------------------------------------------------------------------------

struct PsTuiProcInfo {
    int pid;
    int ppid;
    char comm[256];
    char cmd[4096];
    char state;
    unsigned uid;
    char user[64];
    long tty_nr;
    unsigned long long utime;
    unsigned long long stime;
    long rss;
    unsigned long vsize;
    unsigned long long starttime;
    float cpu_pct;
    float mem_pct;
    int depth;
};

struct PsTuiMemInfo {
    unsigned long total;
    unsigned long available;
};

enum class PsSortMode { CPU, MEM, PID };

static long ps_tui_clk_tck;
static long ps_tui_page_sz;

static PsTuiMemInfo ps_tui_read_meminfo() {
    PsTuiMemInfo info = {0, 0};
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return info;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu kB", &val) == 1) info.total = val;
        else if (sscanf(line, "MemAvailable: %lu kB", &val) == 1) info.available = val;
    }
    fclose(f);
    return info;
}

static float ps_tui_read_uptime() {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up;
    if (fscanf(f, "%lf", &up) != 1) up = 0;
    fclose(f);
    return (float)up;
}

static void ps_tui_read_loadavg(float loads[3]) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return;
    if (fscanf(f, "%f %f %f", &loads[0], &loads[1], &loads[2]) != 3) {
        loads[0] = loads[1] = loads[2] = 0;
    }
    fclose(f);
}

static bool ps_tui_read_proc_status(int pid, unsigned* uid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "Uid: %u", uid) == 1) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

static int ps_tui_read_proc_stat(int pid, PsTuiProcInfo* info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    char buf[4096];
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    const char* start = strchr(buf, '(');
    if (!start) return -1;
    start++;
    const char* end = strrchr(buf, ')');
    if (!end) return -1;

    int comm_len = end - start;
    if (comm_len > 255) comm_len = 255;
    strncpy(info->comm, start, (size_t)comm_len);
    info->comm[comm_len] = '\0';

    const char* p = end + 2;

    char st;
    int ppid, pgrp, sess, tty, tpgid;
    unsigned fl;
    unsigned long minflt, cminflt, majflt, cmajflt;
    unsigned long long utime, stime;
    long priority, nice, num_threads;
    unsigned long long starttime;
    unsigned long vsize;
    long rss;

    if (sscanf(p, "%c %d %d %d %d %d %u %lu %lu %lu %lu %llu %llu %*s %*s %ld %ld %ld %*s %llu %lu %ld",
               &st, &ppid, &pgrp, &sess, &tty, &tpgid,
               &fl,
               &minflt, &cminflt, &majflt, &cmajflt,
               &utime, &stime,
               &priority, &nice, &num_threads,
               &starttime, &vsize, &rss) < 4) {
        return -1;
    }

    info->pid = pid;
    info->ppid = ppid;
    info->state = st;
    info->utime = utime;
    info->stime = stime;
    info->rss = rss;
    info->vsize = vsize;
    info->starttime = starttime;
    info->tty_nr = tty;
    return 0;
}

static void ps_tui_read_cmdline(int pid, char* cmd, size_t cmd_size) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE* f = fopen(path, "r");
    if (!f) {
        cmd[0] = '\0';
        return;
    }
    size_t n = fread(cmd, 1, cmd_size - 1, f);
    fclose(f);
    cmd[n] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (cmd[i] == '\0') cmd[i] = ' ';
    }
    if (n == 0) cmd[0] = '\0';
}

static void ps_tui_lookup_user(unsigned uid,
                                std::unordered_map<unsigned, std::string>& cache,
                                char* out, size_t out_size) {
    auto it = cache.find(uid);
    if (it != cache.end()) {
        snprintf(out, out_size, "%s", it->second.c_str());
        return;
    }
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        snprintf(out, out_size, "%s", pw->pw_name);
        cache[uid] = pw->pw_name;
    } else {
        snprintf(out, out_size, "%u", uid);
        cache[uid] = std::to_string(uid);
    }
}

static std::vector<PsTuiProcInfo> ps_tui_read_procs(
    std::unordered_map<unsigned, std::string>& user_cache,
    PsTuiMemInfo mem,
    float uptime) {
    std::vector<PsTuiProcInfo> procs;

    DIR* dir = opendir("/proc");
    if (!dir) return procs;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) continue;
        bool is_num = true;
        for (const char* p = entry->d_name; *p; p++) {
            if (!isdigit((unsigned char)*p)) { is_num = false; break; }
        }
        if (!is_num) continue;
        int pid = atoi(entry->d_name);

        PsTuiProcInfo info;
        memset(&info, 0, sizeof(info));
        if (ps_tui_read_proc_stat(pid, &info) != 0) continue;

        unsigned uid = 0;
        if (ps_tui_read_proc_status(pid, &uid)) {
            info.uid = uid;
            ps_tui_lookup_user(uid, user_cache, info.user, sizeof(info.user));
        } else {
            info.uid = 0;
            snprintf(info.user, sizeof(info.user), "?");
        }

        ps_tui_read_cmdline(pid, info.cmd, sizeof(info.cmd));
        if (info.cmd[0] == '\0') {
            strncpy(info.cmd, info.comm, sizeof(info.cmd) - 1);
            info.cmd[sizeof(info.cmd) - 1] = '\0';
        }

        // Calculate CPU%
        double elapsed = (double)uptime * (double)ps_tui_clk_tck - (double)info.starttime;
        if (elapsed > 0) {
            info.cpu_pct = (float)(100.0 * (double)(info.utime + info.stime) / elapsed);
        }

        // Calculate MEM%
        if (mem.total > 0) {
            info.mem_pct = 100.0f * (float)(info.rss * ps_tui_page_sz / 1024) / (float)mem.total;
        }

        procs.push_back(info);
    }
    closedir(dir);
    return procs;
}

static void ps_tui_build_tree(std::vector<PsTuiProcInfo>& procs) {
    std::unordered_map<int, std::vector<int>> children;
    for (const auto& p : procs) {
        children[p.ppid].push_back(p.pid);
    }

    std::unordered_map<int, int> pid_to_idx;
    for (int i = 0; i < (int)procs.size(); i++) {
        pid_to_idx[procs[i].pid] = i;
    }

    std::vector<PsTuiProcInfo> sorted;
    sorted.reserve(procs.size());

    std::function<void(int, int)> dfs = [&](int pid, int depth) {
        auto it = pid_to_idx.find(pid);
        if (it == pid_to_idx.end()) return;
        procs[it->second].depth = depth;
        sorted.push_back(procs[it->second]);
        auto& ch = children[pid];
        std::sort(ch.begin(), ch.end());
        for (int c : ch) {
            dfs(c, depth + 1);
        }
    };

    std::vector<int> roots;
    for (const auto& p : procs) {
        if (p.ppid == 0 || pid_to_idx.find(p.ppid) == pid_to_idx.end()) {
            roots.push_back(p.pid);
        }
    }
    std::sort(roots.begin(), roots.end());
    for (int r : roots) {
        dfs(r, 0);
    }

    procs = std::move(sorted);
}

static ftxui::Color ps_tui_gradient(float pct) {
    using namespace ftxui;
    if (pct < 50.0f) {
        uint8_t r = (uint8_t)(pct / 50.0f * 255.0f);
        return Color::RGB(r, 255, 0);
    } else {
        uint8_t g = (uint8_t)((100.0f - pct) / 50.0f * 255.0f);
        return Color::RGB(255, g, 0);
    }
}

static void ps_tui_fmt_time(char* buf, size_t size, unsigned long long ticks) {
    unsigned long total_secs = (unsigned long)(ticks / (unsigned long long)ps_tui_clk_tck);
    long hours = (long)(total_secs / 3600);
    long mins = (long)((total_secs % 3600) / 60);
    long secs = total_secs % 60;
    if (hours > 0) {
        snprintf(buf, size, "%ld:%02ld:%02ld", hours, mins, secs);
    } else {
        snprintf(buf, size, "%ld:%02ld", mins, secs);
    }
}

static void ps_tui_fmt_memsize(char* buf, size_t size, unsigned long kb) {
    if (kb >= 1024 * 1024) {
        snprintf(buf, size, "%.1fT", (double)kb / (1024.0 * 1024.0));
    } else if (kb >= 1024) {
        snprintf(buf, size, "%.1fG", (double)kb / 1024.0);
    } else {
        snprintf(buf, size, "%.0fM", (double)kb);
    }
}

static void ps_tui_tty_name(long tty_nr, char* out, size_t out_size) {
    if (tty_nr == 0) {
        snprintf(out, out_size, "?");
        return;
    }
    int major = (tty_nr >> 8) & 0xfff;
    int minor = (tty_nr & 0xff) | ((tty_nr >> 12) & 0xfff00);

    if (major == 4) {
        snprintf(out, out_size, "tty%d", minor);
    } else if (major == 136 || major == 128) {
        snprintf(out, out_size, "pts/%d", minor);
    } else if (major == 3) {
        snprintf(out, out_size, "ttyS%d", minor);
    } else if (major == 348) {
        snprintf(out, out_size, "pts/%d", minor);
    } else {
        snprintf(out, out_size, "%d/%d", major, minor);
    }
}

static bool ps_tui_matches(const PsTuiProcInfo& p, const std::string& query) {
    if (query.empty()) return true;
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d", p.pid);
    if (strstr(pid_str, q.c_str())) return true;

    std::string user = p.user;
    std::transform(user.begin(), user.end(), user.begin(), ::tolower);
    if (user.find(q) != std::string::npos) return true;

    std::string comm = p.comm;
    std::transform(comm.begin(), comm.end(), comm.begin(), ::tolower);
    if (comm.find(q) != std::string::npos) return true;

    std::string cmd = p.cmd;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    if (cmd.find(q) != std::string::npos) return true;

    return false;
}

struct PsTuiFmtWidth {
    int pid, user, cpu, mem, state, time, tty;
};

class PsTuiComponent : public TuiBase {
    std::vector<PsTuiProcInfo> procs_;
    std::vector<PsTuiProcInfo> filtered_;
    PsTuiMemInfo mem_{};
    float uptime_ = 0;
    float loads_[3]{0};
    std::unordered_map<unsigned, std::string> user_cache_;
    bool tree_mode_ = false;
    PsSortMode sort_by_ = PsSortMode::CPU;
public:
    PsTuiComponent() = default;

    int entries_size() const override { return (int)filtered_.size(); }
    ftxui::Element render_row(int idx) const override;
    void fill_entries() override;
    int header_rows() const override { return 5; }
    bool on_command_key(ftxui::Event event) override;

    PsTuiFmtWidth calc_fmt_widths() const {
        int pid = 3, user = 4, cpu = 4, mem = 4;
        int state = 1, time = 5, tty = 3;

        for (const auto& p : filtered_) {
            int n;
            n = snprintf(nullptr, 0, "%d", p.pid);
            if (n > pid) pid = n;
            n = (int)strlen(p.user);
            if (n > user) user = n;
            n = snprintf(nullptr, 0, "%.1f", (double)p.cpu_pct);
            if (n > cpu) cpu = n;
            n = snprintf(nullptr, 0, "%.1f", (double)p.mem_pct);
            if (n > mem) mem = n;

            char time_str[32];
            ps_tui_fmt_time(time_str, sizeof(time_str), p.utime + p.stime);
            n = (int)strlen(time_str);
            if (n > time) time = n;

            char tty_str[64];
            ps_tui_tty_name(p.tty_nr, tty_str, sizeof(tty_str));
            n = (int)strlen(tty_str);
            if (n > tty) tty = n;
        }

        return {pid, user, cpu, mem, state, time, tty};
    }

    ftxui::Element OnRender() override {
        using namespace ftxui;

        Elements rows;
        char buf[1024];

        // Header: hostname, uptime, load
        time_t now_secs;
        time(&now_secs);
        struct tm* tm_now = localtime(&now_secs);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_now);

        int hours = (int)(uptime_ / 3600);
        int mins = (int)((uptime_ - (float)(hours * 3600)) / 60);

        char hostname[256];
        hostname[0] = '\0';
        gethostname(hostname, sizeof(hostname));

        int total = (int)procs_.size();
        int running = 0;
        for (const auto& p : procs_) {
            if (p.state == 'R') running++;
        }

        auto dot_el = text(" \xe2\x97\x8f ") | color(Color::Green) | bold;
        auto sys_el = text(hostname) | bold | color(Color::Cyan);
        snprintf(buf, sizeof(buf), "  up %d:%02d  ", hours, mins);
        auto uptime_el = text(buf) | color(Color::GrayLight);
        snprintf(buf, sizeof(buf), "load: %.2f %.2f %.2f", loads_[0], loads_[1], loads_[2]);
        auto load_el = text(buf) | color(Color::GrayLight);
        snprintf(buf, sizeof(buf), "Tasks: %d [%d]", total, running);
        auto tasks_el = text(buf) | color(Color::GrayLight);

        rows.push_back(hbox({dot_el, sys_el, uptime_el, load_el, text("  "), tasks_el}) | flex_shrink);

        // Memory bar
        if (mem_.total > 0) {
            int bar_w = 40;
            unsigned long used = mem_.total - mem_.available;
            float used_pct = 100.0f * (float)used / (float)mem_.total;
            int fill = (int)(used_pct * bar_w / 100.0f);
            if (fill > bar_w) fill = bar_w;
            if (fill < 0) fill = 0;

            std::string bar;
            for (int i = 0; i < bar_w; i++) {
                bar += (i < fill) ? "\xe2\x96\x88" : "\xe2\x96\x91";
            }

            char used_str[32], total_str[32];
            ps_tui_fmt_memsize(used_str, sizeof(used_str), used);
            ps_tui_fmt_memsize(total_str, sizeof(total_str), mem_.total);

            auto mem_color = ps_tui_gradient(used_pct);
            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%5.1f%%", (double)used_pct);

            rows.push_back(hbox({
                text("MEM  ") | bold | color(Color::White),
                text(bar) | color(mem_color),
                text(pct_str) | color(Color::GrayLight),
                text(std::string("  ") + used_str + " / " + total_str) | color(Color::GrayLight),
            }));
        }

        // Search bar
        rows.push_back(render_search_bar());

        rows.push_back(separator());

        // Column headers
        auto w = calc_fmt_widths();
        snprintf(buf, sizeof(buf),
            "%*s  %-*s  %*s  %*s  %*s  %*s  %*s  %s",
            w.pid, "PID", w.user, "USER", w.cpu, "CPU%", w.mem, "MEM%",
            w.state, "S", w.time, "TIME", w.tty, "TTY", "COMMAND");
        rows.push_back(text(buf) | bold | color(Color::Yellow));

        // Scrollable list
        rows.push_back(render_list());

        // Footer
        rows.push_back(separator());
        const char* sort_label = "CPU";
        if (sort_by_ == PsSortMode::MEM) sort_label = "MEM";
        else if (sort_by_ == PsSortMode::PID) sort_label = "PID";

        snprintf(buf, sizeof(buf),
                 " Sort: %s  Tree: %s  q:quit  /:search  t:tree  c/m/p:sort  j/k:scroll",
                 sort_label, tree_mode_ ? "on" : "off");
        rows.push_back(text(buf) | color(Color::GrayLight));

        return vbox(std::move(rows)) | flex;
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;

        if (search_mode_) {
            if (event == Event::Escape) {
                search_mode_ = false;
                search_input_.clear();
                return true;
            }
            if (event == Event::Return) {
                search_mode_ = false;
                search_query_ = search_input_;
                search_input_.clear();
                Refresh();
                return true;
            }
            if (handle_search(event)) return true;
            return ComponentBase::OnEvent(event);
        }

        if (handle_nav(event)) return true;

        if (event == Event::Character('q') || event == Event::Character('Q')) {
            if (auto* app = App::Active()) app->Exit();
            return true;
        }
        if (event == Event::Custom) {
            Refresh();
            return true;
        }
        if (on_command_key(event)) return true;
        if (event == Event::Character('/')) {
            search_mode_ = true;
            search_input_ = search_query_;
            return true;
        }
        if (event == Event::Escape) {
            if (!search_query_.empty()) {
                search_query_.clear();
                Refresh();
            }
            return true;
        }
        return ComponentBase::OnEvent(event);
    }
};

static void ps_tui_main() {
    ps_tui_clk_tck = sysconf(_SC_CLK_TCK);
    ps_tui_page_sz = sysconf(_SC_PAGE_SIZE);
    if (ps_tui_clk_tck <= 0) ps_tui_clk_tck = 100;
    if (ps_tui_page_sz <= 0) ps_tui_page_sz = 4096;

    auto screen = ftxui::App::Fullscreen();
    screen.TrackMouse(false);

    auto component = std::make_shared<PsTuiComponent>();
    ftxui::Loop loop(&screen, component);

    component->Refresh();

    double delay = 1.0;
    std::atomic<bool> running{true};
    std::thread refresher([&screen, &running, delay]() {
        const auto step = std::chrono::milliseconds(50);
        const auto interval = std::chrono::milliseconds((int)(delay * 1000));
        auto elapsed = std::chrono::milliseconds(0);
        while (running.load()) {
            std::this_thread::sleep_for(step);
            elapsed += step;
            if (elapsed >= interval) {
                elapsed = std::chrono::milliseconds(0);
                screen.PostEvent(ftxui::Event::Custom);
            }
        }
    });

    loop.Run();

    running.store(false);
    refresher.join();
}

REGISTER_COMMAND("ps", ps_command, "Report process status");

ftxui::Element PsTuiComponent::render_row(int idx) const {
    using namespace ftxui;
    const auto& p = filtered_[idx];

    char buf[1024];
    char tty_str[64];
    ps_tui_tty_name(p.tty_nr, tty_str, sizeof(tty_str));

    char time_str[32];
    ps_tui_fmt_time(time_str, sizeof(time_str), p.utime + p.stime);

    std::string prefix;
    if (tree_mode_ && p.depth > 0) {
        for (int d = 0; d < p.depth && d < 10; d++) {
            prefix += "  ";
        }
        if (p.depth > 0) prefix += "\xe2\x94\x94\xe2\x94\x80";
    }

    std::string cmd_display = prefix + p.comm;
    if (cmd_display.length() > 80) {
        cmd_display = cmd_display.substr(0, 77) + "...";
    }

    auto w = calc_fmt_widths();
    snprintf(buf, sizeof(buf),
        "%*d  %-*s  %*.1f  %*.1f  %*c  %*s  %*s  %s",
        w.pid, p.pid, w.user, p.user, w.cpu, (double)p.cpu_pct,
        w.mem, (double)p.mem_pct, w.state, p.state,
        w.time, time_str, w.tty, tty_str, cmd_display.c_str());

    auto el = text(buf);

    if (p.cpu_pct >= 50.0f) {
        el = el | color(Color::Red);
    } else if (p.cpu_pct >= 10.0f) {
        el = el | color(Color::Yellow);
    } else if (p.cpu_pct >= 1.0f) {
        el = el | color(Color::Green);
    }

    return el;
}

void PsTuiComponent::fill_entries() {
    using namespace ftxui;

    mem_ = ps_tui_read_meminfo();
    uptime_ = ps_tui_read_uptime();
    ps_tui_read_loadavg(loads_);
    procs_ = ps_tui_read_procs(user_cache_, mem_, uptime_);

    if (tree_mode_) {
        ps_tui_build_tree(procs_);
    } else {
        switch (sort_by_) {
        case PsSortMode::CPU:
            std::sort(procs_.begin(), procs_.end(),
                      [](const PsTuiProcInfo& a, const PsTuiProcInfo& b) {
                          return a.cpu_pct > b.cpu_pct;
                      });
            break;
        case PsSortMode::MEM:
            std::sort(procs_.begin(), procs_.end(),
                      [](const PsTuiProcInfo& a, const PsTuiProcInfo& b) {
                          return a.mem_pct > b.mem_pct;
                      });
            break;
        case PsSortMode::PID:
            std::sort(procs_.begin(), procs_.end(),
                      [](const PsTuiProcInfo& a, const PsTuiProcInfo& b) {
                          return a.pid < b.pid;
                      });
            break;
        }
    }

    filtered_.clear();
    for (const auto& p : procs_) {
        if (ps_tui_matches(p, search_query_)) {
            filtered_.push_back(p);
        }
    }

}

bool PsTuiComponent::on_command_key(ftxui::Event event) {
    using namespace ftxui;

    if (event == Event::Character('q') || event == Event::Character('Q')) {
        if (auto* app = App::Active()) app->Exit();
        return true;
    }
    if (event == Event::Character('c') || event == Event::Character('C')) {
        sort_by_ = PsSortMode::CPU;
        tree_mode_ = false;
        Refresh();
        return true;
    }
    if (event == Event::Character('m') || event == Event::Character('M')) {
        sort_by_ = PsSortMode::MEM;
        tree_mode_ = false;
        Refresh();
        return true;
    }
    if (event == Event::Character('p') || event == Event::Character('P')) {
        sort_by_ = PsSortMode::PID;
        tree_mode_ = false;
        Refresh();
        return true;
    }
    if (event == Event::Character('t') || event == Event::Character('T')) {
        tree_mode_ = !tree_mode_;
        Refresh();
        return true;
    }
    if (event == Event::Character('/')) {
        search_mode_ = true;
        search_input_ = search_query_;
        return true;
    }
    if (event == Event::Escape) {
        if (!search_query_.empty()) {
            search_query_.clear();
            Refresh();
        }
        return true;
    }
    return false;
}
