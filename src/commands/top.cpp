#include <algorithm>
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
#include <utility>
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
#include "commands/arg_util.hpp"
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

static MemInfo top_read_meminfo() {
    MemInfo info = {.total=0, .free=0, .available=0};
    FILE* f = fopen("/proc/meminfo", "r");
    if (f == nullptr) { return info;
}
    char line[256];
    while (fgets(line, sizeof(line), f) != nullptr) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu kB", &val) == 1) { info.total = val;
        } else if (sscanf(line, "MemFree: %lu kB", &val) == 1) { info.free = val;
        } else if (sscanf(line, "MemAvailable: %lu kB", &val) == 1) { info.available = val;
}
    }
    (void)fclose(f);
    return info;
}

static float top_read_uptime() {
    FILE* f = fopen("/proc/uptime", "r");
    if (f == nullptr) { return 0;
}
    double up;
    if (fscanf(f, "%lf", &up) != 1) { up = 0;
}
    (void)fclose(f);
    return static_cast<float>(up);
}

static void top_read_loadavg(float loads[3]) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (f == nullptr) { return;
}
    if (fscanf(f, "%f %f %f", &loads[0], &loads[1], &loads[2]) != 3) {
        loads[0] = loads[1] = loads[2] = 0;
    }
    (void)fclose(f);
}

static bool top_read_proc_status(int pid, unsigned* uid) {
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (f == nullptr) { return false;
}
    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), f) != nullptr) {
        if (sscanf(line, "Uid: %u", uid) == 1) {
            found = true;
            break;
        }
    }
    (void)fclose(f);
    return found;
}

static int top_read_proc_stat_line(int pid, ProcInfo* info) {
    char path[64];
    (void)snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE* f = fopen(path, "r");
    if (f == nullptr) { return -1;
}
    char buf[4096];
    if (fgets(buf, sizeof(buf), f) == nullptr) {
        (void)fclose(f);
        return -1;
    }
    (void)fclose(f);

    const char* start = strchr(buf, '(');
    if (start == nullptr) { return -1;
}
    start++;
    const char* end = strrchr(buf, ')');
    if (end == nullptr) { return -1;
}

    int comm_len = end - start;
    comm_len = std::min(comm_len, 255);
    strncpy(info->comm, start, static_cast<size_t>(comm_len));
    info->comm[comm_len] = '\0';

    const char* p = end + 2;

    char st;
    int ppid;
    int pgrp;
    int sess;
    int tty;
    int tpgid;
    unsigned fl;
    unsigned long minflt;
    unsigned long cminflt;
    unsigned long majflt;
    unsigned long cmajflt;
    unsigned long long utime;
    unsigned long long stime;
    long priority;
    long nice;
    long num_threads;
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
    info->priority = static_cast<int>(priority);
    info->nice = static_cast<int>(nice);
    info->utime = utime;
    info->stime = stime;
    info->rss = rss;
    info->vsize = vsize;
    info->starttime = starttime;
    info->num_threads = static_cast<int>(num_threads);
    return 0;
}

static void top_lookup_user(unsigned uid, std::unordered_map<unsigned, std::string>& cache, char* out, size_t out_size) {
    auto it = cache.find(uid);
    if (it != cache.end()) {
        (void)snprintf(out, out_size, "%s", it->second.c_str());
        return;
    }
    const struct passwd* pw = getpwuid(uid);
    if (pw != nullptr) {
        (void)snprintf(out, out_size, "%s", pw->pw_name);
        cache[uid] = pw->pw_name;
    } else {
        (void)snprintf(out, out_size, "%u", uid);
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
                (void)snprintf(info.user, sizeof(info.user), "?");
            }
            procs.push_back(info);
        }
        return procs;
    }

    DIR* dir = opendir("/proc");
    if (dir == nullptr) { return procs;
}

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) { continue;
}
        bool is_num = true;
        for (const char* p = entry->d_name; (*p) != 0; p++) {
            if (isdigit(static_cast<unsigned char>(*p)) == 0) { is_num = false; break; }
        }
        if (!is_num) { continue;
}
        int const pid = atoi(entry->d_name);

        ProcInfo info;
        memset(&info, 0, sizeof(info));
        if (top_read_proc_stat_line(pid, &info) != 0) { continue;
}

        unsigned uid = 0;
        if (top_read_proc_status(pid, &uid)) {
            info.uid = uid;
            top_lookup_user(uid, user_cache, info.user, sizeof(info.user));
        } else {
            info.uid = 0;
            (void)snprintf(info.user, sizeof(info.user), "?");
        }

        procs.push_back(info);
    }
    closedir(dir);
    return procs;
}

static float top_calc_cpu_pct(const ProcInfo& p, float uptime_secs) {
    unsigned long long const total_ticks = p.utime + p.stime;
    double const elapsed = static_cast<double>(uptime_secs) * static_cast<double>(clk_tck) - static_cast<double>(p.starttime);
    if (elapsed <= 0) { return 0;
}
    return static_cast<float>(100.0 * static_cast<double>(total_ticks) / elapsed);
}

static void top_fmt_time(char* buf, size_t size, unsigned long long ticks) {
    unsigned long const total_secs = static_cast<unsigned long>(ticks / static_cast<unsigned long long>(clk_tck));
    unsigned long const mins = total_secs / 60;
    unsigned long const secs = total_secs % 60;
    unsigned long const hsecs = static_cast<unsigned long>((ticks % static_cast<unsigned long long>(clk_tck)) * 100ULL / static_cast<unsigned long long>(clk_tck));
    (void)snprintf(buf, size, "%lu:%02lu.%02lu", mins, secs, hsecs);
}

struct FmtWidth {
    int pid, user, pr, ni, virt, res, shr, cpu, mem, time;
};

static FmtWidth calc_fmt_widths(const std::vector<ProcInfo>& procs) {
    int pid = 3;
    int user = 4;
    int pr = 2;
    int ni = 2;
    int virt = 4;
    int res = 3;
    int const shr = 3;
    int cpu = 4;
    int mem = 4;
    int time = 5;

    for (const auto& p : procs) {
        int n;
        n = snprintf(nullptr, 0, "%d", p.pid);
        pid = std::max(n, pid);
        n = static_cast<int>(strlen(p.user));
        user = std::max(n, user);
        n = snprintf(nullptr, 0, "%d", p.priority);
        pr = std::max(n, pr);
        n = snprintf(nullptr, 0, "%d", p.nice);
        ni = std::max(n, ni);

        unsigned long const virt_kb = p.vsize / 1024;
        n = snprintf(nullptr, 0, "%lu", virt_kb);
        virt = std::max(n, virt);

        unsigned long const res_kb = static_cast<unsigned long>(p.rss * page_sz / 1024);
        n = snprintf(nullptr, 0, "%lu", res_kb);
        res = std::max(n, res);

        char tb[32];
        top_fmt_time(tb, sizeof(tb), p.utime + p.stime);
        n = static_cast<int>(strlen(tb));
        time = std::max(n, time);

        n = snprintf(nullptr, 0, "%.1f", static_cast<double>(p.cpu_pct));
        cpu = std::max(n, cpu);

        n = snprintf(nullptr, 0, "%.1f", static_cast<double>(p.mem_pct));
        mem = std::max(n, mem);
    }

    return {.pid=pid, .user=user, .pr=pr, .ni=ni, .virt=virt, .res=res, .shr=shr, .cpu=cpu, .mem=mem, .time=time};
}

static void fmt_header_line(char* buf, size_t size, const FmtWidth& w) {
    (void)snprintf(buf, size,
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

    unsigned long const virt_kb = p.vsize / 1024;
    unsigned long const res_kb = static_cast<unsigned long>(p.rss * page_sz / 1024);

    (void)snprintf(buf, size,
        "%*d %-*s %*d %*d %*lu %*lu %*lu %c %*.1f %*.1f %*s %s",
        w.pid, p.pid,
        w.user, p.user,
        w.pr, p.priority,
        w.ni, p.nice,
        w.virt, virt_kb,
        w.res, res_kb,
        w.shr, 0UL,
        p.state,
        w.cpu, static_cast<double>(p.cpu_pct),
        w.mem, static_cast<double>(p.mem_pct),
        w.time, timebuf,
        p.comm);
}

static void top_print_snapshot(const std::vector<ProcInfo>& procs, const MemInfo& mem,
                                float uptime, const float loads[3], int max_rows) {
    time_t now_secs;
    (void)time(&now_secs);
    const struct tm* tm_now = localtime(&now_secs);
    char timebuf_hdr[64];
    (void)strftime(timebuf_hdr, sizeof(timebuf_hdr), "%H:%M:%S", tm_now);

    int const hours = static_cast<int>(uptime / 3600);
    int const mins = static_cast<int>((uptime - static_cast<float>(hours * 3600)) / 60);

    printf("top - %s up %d:%02d,  load average: %.2f %.2f %.2f\n",
           timebuf_hdr, hours, mins, loads[0], loads[1], loads[2]);

    int const total = static_cast<int>(procs.size());
    int running = 0;
    int sleeping = 0;
    int stopped = 0;
    int zombie = 0;
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
               static_cast<float>(mem.total) / 1024.0F,
               static_cast<float>(mem.free) / 1024.0F,
               static_cast<float>(mem.total - mem.free) / 1024.0F,
               static_cast<float>(mem.total - mem.free - mem.available) / 1024.0F);
    }

    printf("\n");

    auto w = calc_fmt_widths(procs);
    char hdr[256];
    fmt_header_line(hdr, sizeof(hdr), w);
    printf("%s\n", hdr);

    int display_count = total;
    int const avail_rows = max_rows - 5;
    display_count = std::min(display_count, avail_rows);
    display_count = std::min(display_count, 200);

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
                p.mem_pct = 100.0F * static_cast<float>(p.rss * page_sz / 1024) / static_cast<float>(mem_.total);
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
        (void)time(&now_secs);
        const struct tm* tm_now = localtime(&now_secs);
        char timebuf[64];
        (void)strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_now);

        int const hours = static_cast<int>(uptime_ / 3600);
        int const mins = static_cast<int>((uptime_ - static_cast<float>(hours * 3600)) / 60);

        char buf[256];
        (void)snprintf(buf, sizeof(buf), "top - %s up %d:%02d,  load average: %.2f %.2f %.2f",
                 timebuf, hours, mins, loads_[0], loads_[1], loads_[2]);
        rows.push_back(text(buf));

        int const total = static_cast<int>(procs_.size());
        int running = 0;
        int sleeping = 0;
        int stopped = 0;
        int zombie = 0;
        for (const auto& p : procs_) {
            switch (p.state) {
                case 'R': running++; break;
                case 'S': case 'D': sleeping++; break;
                case 'T': stopped++; break;
                case 'Z': zombie++; break;
                default: break;
            }
        }
        (void)snprintf(buf, sizeof(buf), "Tasks: %d total, %d running, %d sleeping, %d stopped, %d zombie",
                 total, running, sleeping, stopped, zombie);
        rows.push_back(text(buf));

        if (mem_.total > 0) {
            (void)snprintf(buf, sizeof(buf), "MiB Mem: %.1f total, %.1f free, %.1f used, %.1f buff/cache",
                     static_cast<float>(mem_.total) / 1024.0F,
                     static_cast<float>(mem_.free) / 1024.0F,
                     static_cast<float>(mem_.total - mem_.free) / 1024.0F,
                     static_cast<float>(mem_.total - mem_.free - mem_.available) / 1024.0F);
            rows.push_back(text(buf));
        }

        rows.push_back(separator());

        auto w = calc_fmt_widths(procs_);
        fmt_header_line(buf, sizeof(buf), w);
        rows.push_back(text(buf) | bold);

        int display_count = static_cast<int>(procs_.size());
        if (max_rows_ > 0 && display_count > max_rows_) {
            display_count = max_rows_;
}
        display_count = std::min(display_count, 200);

        for (int i = 0; i < display_count; i++) {
            fmt_proc_line(buf, sizeof(buf), procs_[i], w);

            auto el = text(buf);
            if (i % 2 == 0) {
                el = el | bgcolor(Color::Default);
}
            rows.push_back(el);
        }

        auto content = vbox(std::move(rows)) | flex;

        return content;
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            if (auto* app = App::Active()) {
                app->Exit();
}
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

int top_command(int argc, char** argv) {
    clk_tck = sysconf(_SC_CLK_TCK);
    page_sz = sysconf(_SC_PAGE_SIZE);
    if (clk_tck <= 0) { clk_tck = 100;
}
    if (page_sz <= 0) { page_sz = 4096;
}

    struct arg_int* iterations_opt = arg_int0("n", "iterations", "N", "number of iterations (default: 0=TUI, 1=batch)");
    struct arg_dbl* delay_opt = arg_dbl0("d", "delay", "SECS", "delay between updates (default 1.0)");
    struct arg_int* pid_opt = arg_int0("p", "pid", "PID", "monitor only this process");
    struct arg_lit* batch_opt = arg_lit0("b", "batch", "batch mode (no TUI)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);

    ArgTable at({
        iterations_opt, delay_opt, pid_opt, batch_opt, help_opt, end
    });

    int const nerrors = at.parse(argc, argv);

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
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    int const batch = static_cast<int>((batch_opt->count > 0) || (isatty(STDOUT_FILENO)) == 0);
    int const user_set_n = static_cast<int>(iterations_opt->count > 0);

    int iterations;
    if (user_set_n != 0) {
        iterations = iterations_opt->ival[0];
        iterations = std::max(iterations, 0);
    } else {
        iterations = (batch != 0) ? 1 : 0;
    }

    double delay = 1.0;
    if (delay_opt->count > 0) {
        delay = delay_opt->dval[0];
        delay = std::max(delay, 0.1);
    }

    int only_pid = -1;
    if (pid_opt->count > 0) {
        only_pid = pid_opt->ival[0];
        if (only_pid < 1) { only_pid = -1;
}
    }

    int const tui = static_cast<int>((batch == 0) && (isatty(STDOUT_FILENO)) != 0);

    int count = 0;
    std::unordered_map<unsigned, std::string> user_cache;

    if (tui != 0) {
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
            const auto interval = std::chrono::milliseconds(static_cast<int>(delay * 1000));
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
            MemInfo const mem = top_read_meminfo();
            float const uptime = top_read_uptime();
            float loads[3] = {0};
            top_read_loadavg(loads);

            std::vector<ProcInfo> procs = top_read_procs(user_cache, only_pid);

            for (auto& p : procs) {
                p.cpu_pct = top_calc_cpu_pct(p, uptime);
                if (mem.total > 0) {
                    p.mem_pct = 100.0F * static_cast<float>(p.rss * page_sz / 1024) / static_cast<float>(mem.total);
                }
            }

            std::sort(procs.begin(), procs.end(), [](const ProcInfo& a, const ProcInfo& b) {
                return a.cpu_pct > b.cpu_pct;
            });

            int const rows = 9999;
            top_print_snapshot(procs, mem, uptime, loads, rows);

            count++;
            if (iterations != 0 && count >= iterations) { break;
}

            sleep(static_cast<unsigned int>(delay));
        }
    }

    return 0;
}

REGISTER_COMMAND("top", top_command, "Display Linux processes");
