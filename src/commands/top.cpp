#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <ctime>
#include <unistd.h>
#include <dirent.h>
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

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>

#include "commands/top.hpp"
#include "commands/command_macros.hpp"

struct ProcInfo {
    int pid;
    char comm[256];
    char state;
    unsigned uid;
    char user[64];
    int priority;
    int nice;
    unsigned long long utime;
    unsigned long long stime;
    long rss;
    unsigned long vsize;
    unsigned long long starttime;
    int num_threads;
    float cpu_pct;
    float mem_pct;
};

struct MemInfo {
    unsigned long total;
    unsigned long free;
    unsigned long available;
};

static long clk_tck;
static long page_sz;

static MemInfo top_read_meminfo(void) {
    MemInfo info = {0, 0, 0};
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return info;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu kB", &val) == 1) info.total = val;
        else if (sscanf(line, "MemFree: %lu kB", &val) == 1) info.free = val;
        else if (sscanf(line, "MemAvailable: %lu kB", &val) == 1) info.available = val;
    }
    fclose(f);
    return info;
}

static float top_read_uptime(void) {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up;
    if (fscanf(f, "%lf", &up) != 1) up = 0;
    fclose(f);
    return (float)up;
}

static void top_read_loadavg(float loads[3]) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return;
    if (fscanf(f, "%f %f %f", &loads[0], &loads[1], &loads[2]) != 3) {
        loads[0] = loads[1] = loads[2] = 0;
    }
    fclose(f);
}

static bool top_read_proc_status(int pid, unsigned* uid) {
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

static int top_read_proc_stat_line(int pid, ProcInfo* info) {
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
    info->state = st;
    info->priority = (int)priority;
    info->nice = (int)nice;
    info->utime = utime;
    info->stime = stime;
    info->rss = rss;
    info->vsize = vsize;
    info->starttime = starttime;
    info->num_threads = (int)num_threads;
    return 0;
}

static void top_lookup_user(unsigned uid, std::unordered_map<unsigned, std::string>& cache, char* out, size_t out_size) {
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

static std::vector<ProcInfo> top_read_procs(std::unordered_map<unsigned, std::string>& user_cache, int only_pid) {
    std::vector<ProcInfo> procs;

    if (only_pid > 0) {
        ProcInfo info;
        memset(&info, 0, sizeof(info));
        if (top_read_proc_stat_line(only_pid, &info) == 0) {
            unsigned uid = 0;
            if (top_read_proc_status(only_pid, &uid)) {
                info.uid = uid;
                top_lookup_user(uid, user_cache, info.user, sizeof(info.user));
            } else {
                info.uid = 0;
                snprintf(info.user, sizeof(info.user), "?");
            }
            procs.push_back(info);
        }
        return procs;
    }

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

        ProcInfo info;
        memset(&info, 0, sizeof(info));
        if (top_read_proc_stat_line(pid, &info) != 0) continue;

        unsigned uid = 0;
        if (top_read_proc_status(pid, &uid)) {
            info.uid = uid;
            top_lookup_user(uid, user_cache, info.user, sizeof(info.user));
        } else {
            info.uid = 0;
            snprintf(info.user, sizeof(info.user), "?");
        }

        procs.push_back(info);
    }
    closedir(dir);
    return procs;
}

static float top_calc_cpu_pct(const ProcInfo& p, float uptime_secs) {
    unsigned long long total_ticks = p.utime + p.stime;
    double elapsed = (double)uptime_secs * (double)clk_tck - (double)p.starttime;
    if (elapsed <= 0) return 0;
    return (float)(100.0 * (double)total_ticks / elapsed);
}

static void top_fmt_time(char* buf, size_t size, unsigned long long ticks) {
    unsigned long total_secs = (unsigned long)(ticks / (unsigned long long)clk_tck);
    unsigned long mins = total_secs / 60;
    unsigned long secs = total_secs % 60;
    unsigned long hsecs = (unsigned long)((ticks % (unsigned long long)clk_tck) * 100ULL / (unsigned long long)clk_tck);
    snprintf(buf, size, "%lu:%02lu.%02lu", mins, secs, hsecs);
}

struct FmtWidth {
    int pid, user, pr, ni, virt, res, shr, cpu, mem, time;
};

static FmtWidth calc_fmt_widths(const std::vector<ProcInfo>& procs) {
    int pid = 3, user = 4, pr = 2, ni = 2;
    int virt = 4, res = 3, shr = 3;
    int cpu = 4, mem = 4, time = 5;

    for (const auto& p : procs) {
        int n;
        n = snprintf(nullptr, 0, "%d", p.pid);
        if (n > pid) pid = n;
        n = (int)strlen(p.user);
        if (n > user) user = n;
        n = snprintf(nullptr, 0, "%d", (int)p.priority);
        if (n > pr) pr = n;
        n = snprintf(nullptr, 0, "%d", (int)p.nice);
        if (n > ni) ni = n;

        unsigned long virt_kb = p.vsize / 1024;
        n = snprintf(nullptr, 0, "%lu", virt_kb);
        if (n > virt) virt = n;

        unsigned long res_kb = (unsigned long)(p.rss * page_sz / 1024);
        n = snprintf(nullptr, 0, "%lu", res_kb);
        if (n > res) res = n;

        char tb[32];
        top_fmt_time(tb, sizeof(tb), p.utime + p.stime);
        n = (int)strlen(tb);
        if (n > time) time = n;

        n = snprintf(nullptr, 0, "%.1f", (double)p.cpu_pct);
        if (n > cpu) cpu = n;

        n = snprintf(nullptr, 0, "%.1f", (double)p.mem_pct);
        if (n > mem) mem = n;
    }

    return {pid, user, pr, ni, virt, res, shr, cpu, mem, time};
}

static void fmt_header_line(char* buf, size_t size, const FmtWidth& w) {
    snprintf(buf, size,
        "%*s %-*s %*s %*s %*s %*s %*s %c %*s %*s %*s %s",
        w.pid, "PID",
        w.user, "USER",
        w.pr, "PR",
        w.ni, "NI",
        w.virt, "VIRT",
        w.res, "RES",
        w.shr, "SHR",
        'S',
        w.cpu, "%CPU",
        w.mem, "%MEM",
        w.time, "TIME+",
        "COMMAND");
}

static void fmt_proc_line(char* buf, size_t size, const ProcInfo& p, const FmtWidth& w) {
    char timebuf[32];
    top_fmt_time(timebuf, sizeof(timebuf), p.utime + p.stime);

    unsigned long virt_kb = p.vsize / 1024;
    unsigned long res_kb = (unsigned long)(p.rss * page_sz / 1024);

    snprintf(buf, size,
        "%*d %-*s %*d %*d %*lu %*lu %*lu %c %*.1f %*.1f %*s %s",
        w.pid, p.pid,
        w.user, p.user,
        w.pr, p.priority,
        w.ni, p.nice,
        w.virt, virt_kb,
        w.res, res_kb,
        w.shr, 0UL,
        p.state,
        w.cpu, (double)p.cpu_pct,
        w.mem, (double)p.mem_pct,
        w.time, timebuf,
        p.comm);
}

static void top_print_snapshot(const std::vector<ProcInfo>& procs, const MemInfo& mem,
                                float uptime, const float loads[3], int max_rows) {
    time_t now_secs;
    time(&now_secs);
    struct tm* tm_now = localtime(&now_secs);
    char timebuf_hdr[64];
    strftime(timebuf_hdr, sizeof(timebuf_hdr), "%H:%M:%S", tm_now);

    int hours = (int)(uptime / 3600);
    int mins = (int)((uptime - (float)(hours * 3600)) / 60);

    printf("top - %s up %d:%02d,  load average: %.2f %.2f %.2f\n",
           timebuf_hdr, hours, mins, loads[0], loads[1], loads[2]);

    int total = (int)procs.size();
    int running = 0, sleeping = 0, stopped = 0, zombie = 0;
    for (const auto& p : procs) {
        switch (p.state) {
            case 'R': running++; break;
            case 'S': case 'D': sleeping++; break;
            case 'T': stopped++; break;
            case 'Z': zombie++; break;
            default: break;
        }
    }
    printf("Tasks: %d total, %d running, %d sleeping, %d stopped, %d zombie\n",
           total, running, sleeping, stopped, zombie);

    if (mem.total > 0) {
        printf("MiB Mem: %.1f total, %.1f free, %.1f used, %.1f buff/cache\n",
               (float)mem.total / 1024.0f,
               (float)mem.free / 1024.0f,
               (float)(mem.total - mem.free) / 1024.0f,
               (float)(mem.total - mem.free - mem.available) / 1024.0f);
    }

    printf("\n");

    auto w = calc_fmt_widths(procs);
    char hdr[256];
    fmt_header_line(hdr, sizeof(hdr), w);
    printf("%s\n", hdr);

    int display_count = total;
    int avail_rows = max_rows - 5;
    if (display_count > avail_rows) display_count = avail_rows;
    if (display_count > 200) display_count = 200;

    for (int i = 0; i < display_count; i++) {
        char line[384];
        fmt_proc_line(line, sizeof(line), procs[i], w);
        printf("%s\n", line);
    }
}

class TopComponent : public ftxui::ComponentBase {
    std::vector<ProcInfo> procs_;
    MemInfo mem_{};
    float uptime_ = 0;
    float loads_[3]{0};
    std::unordered_map<unsigned, std::string> user_cache_;
    int only_pid_ = -1;
    int max_rows_ = 0;

public:
    TopComponent(int only_pid) : only_pid_(only_pid) {}

    void Refresh() {
        mem_ = top_read_meminfo();
        uptime_ = top_read_uptime();
        top_read_loadavg(loads_);
        procs_ = top_read_procs(user_cache_, only_pid_);

        for (auto& p : procs_) {
            p.cpu_pct = top_calc_cpu_pct(p, uptime_);
            if (mem_.total > 0) {
                p.mem_pct = 100.0f * (float)(p.rss * page_sz / 1024) / (float)mem_.total;
            }
        }

        std::sort(procs_.begin(), procs_.end(), [](const ProcInfo& a, const ProcInfo& b) {
            return a.cpu_pct > b.cpu_pct;
        });

        if (auto* app = ftxui::App::Active()) {
            max_rows_ = app->dimy() - 6;
        }
    }

    ftxui::Element OnRender() override {
        using namespace ftxui;

        Elements rows;

        time_t now_secs;
        time(&now_secs);
        struct tm* tm_now = localtime(&now_secs);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_now);

        int hours = (int)(uptime_ / 3600);
        int mins = (int)((uptime_ - (float)(hours * 3600)) / 60);

        char buf[256];
        snprintf(buf, sizeof(buf), "top - %s up %d:%02d,  load average: %.2f %.2f %.2f",
                 timebuf, hours, mins, loads_[0], loads_[1], loads_[2]);
        rows.push_back(text(buf));

        int total = (int)procs_.size();
        int running = 0, sleeping = 0, stopped = 0, zombie = 0;
        for (const auto& p : procs_) {
            switch (p.state) {
                case 'R': running++; break;
                case 'S': case 'D': sleeping++; break;
                case 'T': stopped++; break;
                case 'Z': zombie++; break;
                default: break;
            }
        }
        snprintf(buf, sizeof(buf), "Tasks: %d total, %d running, %d sleeping, %d stopped, %d zombie",
                 total, running, sleeping, stopped, zombie);
        rows.push_back(text(buf));

        if (mem_.total > 0) {
            snprintf(buf, sizeof(buf), "MiB Mem: %.1f total, %.1f free, %.1f used, %.1f buff/cache",
                     (float)mem_.total / 1024.0f,
                     (float)mem_.free / 1024.0f,
                     (float)(mem_.total - mem_.free) / 1024.0f,
                     (float)(mem_.total - mem_.free - mem_.available) / 1024.0f);
            rows.push_back(text(buf));
        }

        rows.push_back(separator());

        auto w = calc_fmt_widths(procs_);
        fmt_header_line(buf, sizeof(buf), w);
        rows.push_back(text(buf) | bold);

        int display_count = (int)procs_.size();
        if (max_rows_ > 0 && display_count > max_rows_)
            display_count = max_rows_;
        if (display_count > 200)
            display_count = 200;

        for (int i = 0; i < display_count; i++) {
            fmt_proc_line(buf, sizeof(buf), procs_[i], w);

            auto el = text(buf);
            if (i % 2 == 0)
                el = el | bgcolor(Color::Default);
            rows.push_back(el);
        }

        auto content = vbox(std::move(rows)) | flex;

        return content;
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            if (auto* app = App::Active())
                app->Exit();
            return true;
        }
        // Periodic refresh posted by the background timer thread. This decouples
        // data refresh latency from input latency, so keystrokes stay responsive.
        if (event == Event::Custom) {
            Refresh();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }
};

void top_command(int argc, char** argv) {
    clk_tck = sysconf(_SC_CLK_TCK);
    page_sz = sysconf(_SC_PAGE_SIZE);
    if (clk_tck <= 0) clk_tck = 100;
    if (page_sz <= 0) page_sz = 4096;

    struct arg_int* iterations_opt = arg_int0("n", "iterations", "N", "number of iterations (default: 0=TUI, 1=batch)");
    struct arg_dbl* delay_opt = arg_dbl0("d", "delay", "SECS", "delay between updates (default 1.0)");
    struct arg_int* pid_opt = arg_int0("p", "pid", "PID", "monitor only this process");
    struct arg_lit* batch_opt = arg_lit0("b", "batch", "batch mode (no TUI)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        iterations_opt, delay_opt, pid_opt, batch_opt, help_opt, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Display Linux processes.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -n, --iterations=N   number of iterations (default: 0=TUI, 1=batch)\n");
        printf("  -d, --delay=SECS     delay between updates (default 1.0)\n");
        printf("  -p, --pid=PID        monitor only this PID\n");
        printf("  -b, --batch          batch mode (no TUI)\n");
        printf("  -h, --help           display this help and exit\n");
        printf("\n");
        printf("In TUI mode: press 'q' to quit.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int batch = (batch_opt->count > 0) || !isatty(STDOUT_FILENO);
    int user_set_n = (iterations_opt->count > 0);

    int iterations;
    if (user_set_n) {
        iterations = iterations_opt->ival[0];
        if (iterations < 0) iterations = 0;
    } else {
        iterations = batch ? 1 : 0;
    }

    double delay = 1.0;
    if (delay_opt->count > 0) {
        delay = delay_opt->dval[0];
        if (delay < 0.1) delay = 0.1;
    }

    int only_pid = -1;
    if (pid_opt->count > 0) {
        only_pid = pid_opt->ival[0];
        if (only_pid < 1) only_pid = -1;
    }

    int tui = !batch && isatty(STDOUT_FILENO);

    int count = 0;
    std::unordered_map<unsigned, std::string> user_cache;

    if (tui) {
        auto screen = ftxui::App::Fullscreen();
        screen.TrackMouse(false);

        auto component = std::make_shared<TopComponent>(only_pid);
        ftxui::Loop loop(&screen, component);

        component->Refresh();

        // Background thread posts Event::Custom every `delay` seconds to trigger
        // a data refresh + redraw. The main loop blocks on stdin via select(),
        // so keystrokes (q/etc.) are handled with minimal latency instead of
        // waiting for the refresh interval. Without this, every keypress would
        // be delayed by up to `delay` seconds (default 1.0s) while the main
        // thread sleeps.
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
    } else {
        while (iterations == 0 || count < iterations) {
            MemInfo mem = top_read_meminfo();
            float uptime = top_read_uptime();
            float loads[3] = {0};
            top_read_loadavg(loads);

            std::vector<ProcInfo> procs = top_read_procs(user_cache, only_pid);

            for (auto& p : procs) {
                p.cpu_pct = top_calc_cpu_pct(p, uptime);
                if (mem.total > 0) {
                    p.mem_pct = 100.0f * (float)(p.rss * page_sz / 1024) / (float)mem.total;
                }
            }

            std::sort(procs.begin(), procs.end(), [](const ProcInfo& a, const ProcInfo& b) {
                return a.cpu_pct > b.cpu_pct;
            });

            int rows = 9999;
            top_print_snapshot(procs, mem, uptime, loads, rows);

            count++;
            if (iterations != 0 && count >= iterations) break;

            sleep((unsigned int)delay);
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("top", top_command, "Display Linux processes");
