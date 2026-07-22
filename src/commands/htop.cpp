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
#include <cmath>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>

#include "commands/htop.hpp"
#include "commands/command_macros.hpp"

struct HtopProcInfo {
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
    unsigned long long total_cpu_ticks;
};

struct HtopMemInfo {
    unsigned long total;
    unsigned long free;
    unsigned long available;
};

struct HtopCpuInfo {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
};

struct HtopFmtWidth {
    int pid, user, state, cpu, mem, time;
};

static long htop_clk_tck;
static long htop_page_sz;
static int htop_ncpu;

static HtopMemInfo htop_read_meminfo(void) {
    HtopMemInfo info = {0, 0, 0};
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

static float htop_read_uptime(void) {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up;
    if (fscanf(f, "%lf", &up) != 1) up = 0;
    fclose(f);
    return (float)up;
}

static void htop_read_loadavg(float loads[3]) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return;
    if (fscanf(f, "%f %f %f", &loads[0], &loads[1], &loads[2]) != 3) {
        loads[0] = loads[1] = loads[2] = 0;
    }
    fclose(f);
}

static bool htop_read_proc_status(int pid, unsigned* uid) {
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

static int htop_read_proc_stat_line(int pid, HtopProcInfo* info) {
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
    info->total_cpu_ticks = utime + stime;
    return 0;
}

static void htop_lookup_user(unsigned uid, std::unordered_map<unsigned, std::string>& cache, char* out, size_t out_size) {
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

static std::vector<HtopProcInfo> htop_read_procs(std::unordered_map<unsigned, std::string>& user_cache, int only_pid) {
    std::vector<HtopProcInfo> procs;

    if (only_pid > 0) {
        HtopProcInfo info;
        memset(&info, 0, sizeof(info));
        if (htop_read_proc_stat_line(only_pid, &info) == 0) {
            unsigned uid = 0;
            if (htop_read_proc_status(only_pid, &uid)) {
                info.uid = uid;
                htop_lookup_user(uid, user_cache, info.user, sizeof(info.user));
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

        HtopProcInfo info;
        memset(&info, 0, sizeof(info));
        if (htop_read_proc_stat_line(pid, &info) != 0) continue;

        unsigned uid = 0;
        if (htop_read_proc_status(pid, &uid)) {
            info.uid = uid;
            htop_lookup_user(uid, user_cache, info.user, sizeof(info.user));
        } else {
            info.uid = 0;
            snprintf(info.user, sizeof(info.user), "?");
        }

        procs.push_back(info);
    }
    closedir(dir);
    return procs;
}

// Read per-CPU stats from /proc/stat
static std::vector<HtopCpuInfo> htop_read_cpu_stats() {
    std::vector<HtopCpuInfo> stats;
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return stats;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        HtopCpuInfo cpu = {0};
        char prefix[16];
        if (sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                   prefix,
                   &cpu.user, &cpu.nice, &cpu.system, &cpu.idle,
                   &cpu.iowait, &cpu.irq, &cpu.softirq, &cpu.steal) >= 5) {
            if (strncmp(prefix, "cpu", 3) == 0 && prefix[3] >= '0' && prefix[3] <= '9') {
                stats.push_back(cpu);
            }
        }
    }
    fclose(f);
    return stats;
}

static unsigned long long htop_cpu_total(const HtopCpuInfo& c) {
    return c.user + c.nice + c.system + c.idle + c.iowait + c.irq + c.softirq + c.steal;
}

static float htop_calc_cpu_pct(unsigned long long total_ticks, float uptime_secs, unsigned long long starttime) {
    double elapsed = (double)uptime_secs * (double)htop_clk_tck - (double)starttime;
    if (elapsed <= 0) return 0;
    return (float)(100.0 * (double)total_ticks / elapsed);
}

static void htop_fmt_time(char* buf, size_t size, unsigned long long ticks) {
    unsigned long total_secs = (unsigned long)(ticks / (unsigned long long)htop_clk_tck);
    unsigned long hsecs = (unsigned long)((ticks % (unsigned long long)htop_clk_tck) * 100ULL / (unsigned long long)htop_clk_tck);
    unsigned long mins = total_secs / 60;
    unsigned long secs = total_secs % 60;
    snprintf(buf, size, "%lu:%02lu.%02lu", mins, secs, hsecs);
}

static HtopFmtWidth calc_fmt_widths(const std::vector<HtopProcInfo>& procs) {
    int pid = 3, user = 4, state = 1, cpu = 4, mem = 4, time = 5;
    for (const auto& p : procs) {
        int n;
        n = snprintf(nullptr, 0, "%d", p.pid);
        if (n > pid) pid = n;
        n = (int)strlen(p.user);
        if (n > user) user = n;
        char tb[32];
        htop_fmt_time(tb, sizeof(tb), p.total_cpu_ticks);
        n = (int)strlen(tb);
        if (n > time) time = n;
        n = snprintf(nullptr, 0, "%.1f", (double)p.cpu_pct);
        if (n > cpu) cpu = n;
        n = snprintf(nullptr, 0, "%.1f", (double)p.mem_pct);
        if (n > mem) mem = n;
    }
    return {pid, user, state, cpu, mem, time};
}

static void fmt_header_line(char* buf, size_t size, const HtopFmtWidth& w) {
    snprintf(buf, size,
        "%*s %-*s %*s %*s %*s %*s  %s",
        w.pid, "PID",
        w.user, "USER",
        w.state, "S",
        w.cpu, "%CPU",
        w.mem, "%MEM",
        w.time, "TIME",
        "COMMAND");
}

static void fmt_proc_line(char* buf, size_t size, const HtopProcInfo& p, const HtopFmtWidth& w) {
    char timebuf[32];
    htop_fmt_time(timebuf, sizeof(timebuf), p.total_cpu_ticks);

    snprintf(buf, size,
        "%*d %-*s %*c %*.1f %*.1f %*s  %s",
        w.pid, p.pid,
        w.user, p.user,
        w.state, p.state,
        w.cpu, (double)p.cpu_pct,
        w.mem, (double)p.mem_pct,
        w.time, timebuf,
        p.comm);
}

static void htop_make_bar(std::string& out, int width, int filled, const char* full, const char* empty) {
    out.clear();
    for (int j = 0; j < width; j++) {
        out += (j < filled) ? full : empty;
    }
}

class HtopComponent : public ftxui::ComponentBase {
    std::vector<HtopProcInfo> procs_;
    HtopMemInfo mem_{};
    float uptime_ = 0;
    float loads_[3]{0};
    std::vector<HtopCpuInfo> cpu_stats_;
    std::vector<HtopCpuInfo> cpu_stats_prev_;
    std::unordered_map<unsigned, std::string> user_cache_;
    int only_pid_ = -1;
    int scroll_offset_ = 0;
    int max_rows_ = 0;
    int scroll_max_ = 0;
    HtopFmtWidth fmt_w_{5, 8, 1, 5, 5, 7};

public:
    HtopComponent(int only_pid) : only_pid_(only_pid) {}

    void Refresh() {
        mem_ = htop_read_meminfo();
        uptime_ = htop_read_uptime();
        htop_read_loadavg(loads_);

        cpu_stats_prev_ = cpu_stats_;
        cpu_stats_ = htop_read_cpu_stats();

        procs_ = htop_read_procs(user_cache_, only_pid_);

        for (auto& p : procs_) {
            p.cpu_pct = htop_calc_cpu_pct(p.total_cpu_ticks, uptime_, p.starttime);
            if (mem_.total > 0) {
                p.mem_pct = 100.0f * (float)(p.rss * htop_page_sz / 1024) / (float)mem_.total;
            }
        }

        std::sort(procs_.begin(), procs_.end(), [](const HtopProcInfo& a, const HtopProcInfo& b) {
            return a.cpu_pct > b.cpu_pct;
        });

        fmt_w_ = calc_fmt_widths(procs_);

        if (auto* app = ftxui::App::Active()) {
            int h = app->dimy();
            int header_rows = 2 + htop_ncpu + 2;
            max_rows_ = h - header_rows - 2;
            if (max_rows_ < 1) max_rows_ = 1;
        }

        int total = (int)procs_.size();
        scroll_max_ = total - max_rows_;
        if (scroll_max_ < 0) scroll_max_ = 0;
        if (scroll_offset_ > scroll_max_) scroll_offset_ = scroll_max_;
    }

    ftxui::Element OnRender() override {
        using namespace ftxui;

        Elements rows;
        char buf[1024];

        // --- Header: time, uptime, load ---
        time_t now_secs;
        time(&now_secs);
        struct tm* tm_now = localtime(&now_secs);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_now);

        int hours = (int)(uptime_ / 3600);
        int mins = (int)((uptime_ - (float)(hours * 3600)) / 60);

        snprintf(buf, sizeof(buf), "htop - %s up %d:%02d,  load average: %.2f %.2f %.2f",
                 timebuf, hours, mins, loads_[0], loads_[1], loads_[2]);
        rows.push_back(text(buf) | bold | color(Color::Cyan));

        // --- Per-CPU bars ---
        for (int i = 0; i < (int)cpu_stats_.size() && i < 64; i++) {
            float pct = 0;
            if (i < (int)cpu_stats_prev_.size()) {
                unsigned long long total_now = htop_cpu_total(cpu_stats_[i]);
                unsigned long long total_prev = htop_cpu_total(cpu_stats_prev_[i]);
                unsigned long long idle_now = cpu_stats_[i].idle;
                unsigned long long idle_prev = cpu_stats_prev_[i].idle;
                unsigned long long dtotal = total_now - total_prev;
                unsigned long long didle = idle_now - idle_prev;
                if (dtotal > 0) {
                    pct = 100.0f * (float)(dtotal - didle) / (float)dtotal;
                }
            }

            int bar_w = 40;
            int fill = (int)(pct * bar_w / 100.0f);
            if (fill > bar_w) fill = bar_w;
            if (fill < 0) fill = 0;

            std::string bar;
            htop_make_bar(bar, bar_w, fill, "\u2588", "\u2591");

            Color bar_color;
            if (pct < 50.0f) bar_color = Color::Green;
            else if (pct < 75.0f) bar_color = Color::Yellow;
            else bar_color = Color::Red;

            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%.1f%%", (double)pct);

            char label[16];
            if (htop_ncpu > 1) {
                snprintf(label, sizeof(label), "CPU%d", i);
            } else {
                snprintf(label, sizeof(label), "CPU");
            }

            auto lbl = text(std::string(label) + "  ");
            auto bar_el = text(bar) | color(bar_color);
            auto pct_el = text(std::string(" ") + pct_str);

            rows.push_back(hbox({lbl, bar_el, pct_el}));
        }

        // --- Memory bar ---
        if (mem_.total > 0) {
            int bar_w = 40;
            unsigned long used = mem_.total - mem_.available;
            float used_pct = 100.0f * (float)used / (float)mem_.total;
            int fill = (int)(used_pct * bar_w / 100.0f);
            if (fill > bar_w) fill = bar_w;
            if (fill < 0) fill = 0;

            std::string bar;
            htop_make_bar(bar, bar_w, fill, "\u2588", "\u2591");

            Color bar_color;
            if (used_pct < 50.0f) bar_color = Color::Green;
            else if (used_pct < 75.0f) bar_color = Color::Yellow;
            else bar_color = Color::Red;

            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%.1f%%", (double)used_pct);
            char mem_str[48];
            snprintf(mem_str, sizeof(mem_str), "[%.0fM/%.0fM]", (float)used / 1024.0f, (float)mem_.total / 1024.0f);

            rows.push_back(hbox({
                text("Mem  "),
                text(bar) | color(bar_color),
                text(std::string(" ") + pct_str + " " + mem_str)
            }));
        }

        // --- Separator ---
        rows.push_back(separator());

        // --- Column headers ---
        fmt_header_line(buf, sizeof(buf), fmt_w_);
        rows.push_back(text(buf) | bold | color(Color::Yellow));

        // --- Process rows ---
        int display_count = (int)procs_.size();
        int avail = max_rows_;
        if (display_count > avail) display_count = avail;

        for (int i = 0; i < display_count; i++) {
            int idx = i + scroll_offset_;
            if (idx >= (int)procs_.size()) break;

            const auto& p = procs_[idx];

            fmt_proc_line(buf, sizeof(buf), p, fmt_w_);

            Color line_color;
            if (p.cpu_pct >= 50.0f) line_color = Color::Red;
            else if (p.cpu_pct >= 10.0f) line_color = Color::Yellow;
            else line_color = Color::Default;

            auto el = text(buf);
            if (p.cpu_pct >= 1.0f) {
                el = el | color(line_color);
            }
            if (i % 2 == 1) {
                el = el | bgcolor(Color::GrayDark);
            }
            rows.push_back(el);
        }

        // --- Bottom status line ---
        rows.push_back(separator());
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
        snprintf(buf, sizeof(buf), "Tasks: %d, %d running  |  q:quit  j/k:scroll  PgUp/PgDn:page",
                 total, running);
        rows.push_back(text(buf));

        return vbox(std::move(rows)) | flex;
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            if (auto* app = App::Active())
                app->Exit();
            return true;
        }
        // Periodic refresh posted by the background timer thread. This decouples
        // data refresh latency from input latency, so j/k stay responsive.
        if (event == Event::Custom) {
            Refresh();
            return true;
        }
        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (scroll_offset_ < scroll_max_) {
                scroll_offset_++;
            }
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (scroll_offset_ > 0) {
                scroll_offset_--;
            }
            return true;
        }
        if (event == Event::PageDown) {
            scroll_offset_ += max_rows_ / 2;
            if (scroll_offset_ > scroll_max_) scroll_offset_ = scroll_max_;
            return true;
        }
        if (event == Event::PageUp) {
            scroll_offset_ -= max_rows_ / 2;
            if (scroll_offset_ < 0) scroll_offset_ = 0;
            return true;
        }
        if (event == Event::Home) {
            scroll_offset_ = 0;
            return true;
        }
        if (event == Event::End) {
            scroll_offset_ = scroll_max_;
            return true;
        }
        return ComponentBase::OnEvent(event);
    }
};

void htop_command(int argc, char** argv) {
    htop_clk_tck = sysconf(_SC_CLK_TCK);
    htop_page_sz = sysconf(_SC_PAGE_SIZE);
    if (htop_clk_tck <= 0) htop_clk_tck = 100;
    if (htop_page_sz <= 0) htop_page_sz = 4096;

    htop_ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (htop_ncpu < 1) htop_ncpu = 1;

    struct arg_dbl* delay_opt = arg_dbl0("d", "delay", "SECS", "delay between updates (default 1.0)");
    struct arg_int* pid_opt = arg_int0("p", "pid", "PID", "monitor only this process");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        delay_opt, pid_opt, help_opt, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Interactive process viewer (htop-style TUI).\n");
        printf("\n");
        printf("Options:\n");
        printf("  -d, --delay=SECS     delay between updates (default 1.0)\n");
        printf("  -p, --pid=PID        monitor only this PID\n");
        printf("  -h, --help           display this help and exit\n");
        printf("\n");
        printf("Keys:\n");
        printf("  q         Quit\n");
        printf("  j / Down  Scroll down\n");
        printf("  k / Up    Scroll up\n");
        printf("  PgUp      Scroll up half page\n");
        printf("  PgDn      Scroll down half page\n");
        printf("  Home      Scroll to top\n");
        printf("  End       Scroll to bottom\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
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

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    auto screen = ftxui::App::Fullscreen();
    screen.TrackMouse(false);

    auto component = std::make_shared<HtopComponent>(only_pid);
    ftxui::Loop loop(&screen, component);

    htop_read_cpu_stats();

    component->Refresh();

    // Background thread posts Event::Custom every `delay` seconds to trigger a
    // data refresh + redraw. The main loop blocks on stdin via select(), so
    // keystrokes (j/k/etc.) are handled with minimal latency instead of waiting
    // for the refresh interval. Without this, every keypress would be delayed by
    // up to `delay` seconds (default 1.0s) while the main thread sleeps.
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

REGISTER_COMMAND("htop", htop_command, "Interactive process viewer (htop-style TUI)");
