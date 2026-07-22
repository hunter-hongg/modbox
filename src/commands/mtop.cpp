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

#include "commands/mtop.hpp"
#include "commands/command_macros.hpp"

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct MtopProcInfo {
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

struct MtopMemInfo {
    unsigned long total;
    unsigned long free;
    unsigned long available;
};

struct MtopCpuInfo {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
};

struct MtopFmtWidth {
    int pid, user, state, cpu, mem, time;
};

// ---------------------------------------------------------------------------
// Global state (process-wide)
// ---------------------------------------------------------------------------

static long mtop_clk_tck;
static long mtop_page_sz;
static int  mtop_ncpu;

// ---------------------------------------------------------------------------
// /proc helpers
// ---------------------------------------------------------------------------

static MtopMemInfo mtop_read_meminfo(void) {
    MtopMemInfo info = {0, 0, 0};
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

static float mtop_read_uptime(void) {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up;
    if (fscanf(f, "%lf", &up) != 1) up = 0;
    fclose(f);
    return (float)up;
}

static void mtop_read_loadavg(float loads[3]) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return;
    if (fscanf(f, "%f %f %f", &loads[0], &loads[1], &loads[2]) != 3) {
        loads[0] = loads[1] = loads[2] = 0;
    }
    fclose(f);
}

static bool mtop_read_proc_status(int pid, unsigned* uid) {
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

static int mtop_read_proc_stat_line(int pid, MtopProcInfo* info) {
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

static void mtop_lookup_user(unsigned uid,
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

static std::vector<MtopProcInfo>
mtop_read_procs(std::unordered_map<unsigned, std::string>& user_cache,
                int only_pid) {
    std::vector<MtopProcInfo> procs;

    if (only_pid > 0) {
        MtopProcInfo info;
        memset(&info, 0, sizeof(info));
        if (mtop_read_proc_stat_line(only_pid, &info) == 0) {
            unsigned uid = 0;
            if (mtop_read_proc_status(only_pid, &uid)) {
                info.uid = uid;
                mtop_lookup_user(uid, user_cache, info.user, sizeof(info.user));
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

        MtopProcInfo info;
        memset(&info, 0, sizeof(info));
        if (mtop_read_proc_stat_line(pid, &info) != 0) continue;

        unsigned uid = 0;
        if (mtop_read_proc_status(pid, &uid)) {
            info.uid = uid;
            mtop_lookup_user(uid, user_cache, info.user, sizeof(info.user));
        } else {
            info.uid = 0;
            snprintf(info.user, sizeof(info.user), "?");
        }
        procs.push_back(info);
    }
    closedir(dir);
    return procs;
}

static std::vector<MtopCpuInfo> mtop_read_cpu_stats() {
    std::vector<MtopCpuInfo> stats;
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return stats;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        MtopCpuInfo cpu = {0};
        char prefix[16];
        if (sscanf(line,
                   "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                   prefix,
                   &cpu.user, &cpu.nice, &cpu.system, &cpu.idle,
                   &cpu.iowait, &cpu.irq, &cpu.softirq, &cpu.steal) >= 5) {
            if (strncmp(prefix, "cpu", 3) == 0 &&
                prefix[3] >= '0' && prefix[3] <= '9') {
                stats.push_back(cpu);
            }
        }
    }
    fclose(f);
    return stats;
}

static unsigned long long mtop_cpu_total(const MtopCpuInfo& c) {
    return c.user + c.nice + c.system + c.idle +
           c.iowait + c.irq + c.softirq + c.steal;
}

static float mtop_calc_cpu_pct(unsigned long long total_ticks,
                                float uptime_secs,
                                unsigned long long starttime) {
    double elapsed = (double)uptime_secs * (double)mtop_clk_tck -
                     (double)starttime;
    if (elapsed <= 0) return 0;
    return (float)(100.0 * (double)total_ticks / elapsed);
}

static void mtop_fmt_time(char* buf, size_t size, unsigned long long ticks) {
    unsigned long total_secs =
        (unsigned long)(ticks / (unsigned long long)mtop_clk_tck);
    unsigned long hsecs =
        (unsigned long)((ticks % (unsigned long long)mtop_clk_tck) * 100ULL /
                        (unsigned long long)mtop_clk_tck);
    unsigned long mins = total_secs / 60;
    unsigned long secs = total_secs % 60;
    snprintf(buf, size, "%lu:%02lu.%02lu", mins, secs, hsecs);
}

// ---------------------------------------------------------------------------
// Btop-style helpers
// ---------------------------------------------------------------------------

// Map 0–100 → RGB gradient: green → yellow → red
static ftxui::Color gradient_color(float pct) {
    using namespace ftxui;
    if (pct < 50.0f) {
        uint8_t r = (uint8_t)(pct / 50.0f * 255.0f);
        return Color::RGB(r, 255, 0);
    } else {
        uint8_t g = (uint8_t)((100.0f - pct) / 50.0f * 255.0f);
        return Color::RGB(255, g, 0);
    }
}

// Build bar string: filled block █ for used portion, light shade ░ for empty
static void make_bar(std::string& out, int width, int filled) {
    out.clear();
    for (int i = 0; i < width; i++) {
        out += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";
    }
}

// Format memory size in human-friendly units
static void fmt_memsize(char* buf, size_t size, unsigned long kb) {
    if (kb >= 1024 * 1024) {
        snprintf(buf, size, "%.1fT", (double)kb / (1024.0 * 1024.0));
    } else if (kb >= 1024) {
        snprintf(buf, size, "%.1fG", (double)kb / 1024.0);
    } else if (kb >= 1) {
        snprintf(buf, size, "%.0fM", (double)kb);
    } else {
        snprintf(buf, size, "0M");
    }
}

// ---------------------------------------------------------------------------
// Table alignment helpers (htop pattern)
// ---------------------------------------------------------------------------

static MtopFmtWidth calc_fmt_widths(const std::vector<MtopProcInfo>& procs) {
    int pid = 3, user = 4, state = 1, cpu = 4, mem = 4, time = 5;
    for (const auto& p : procs) {
        int n;
        n = snprintf(nullptr, 0, "%d", p.pid);
        if (n > pid) pid = n;
        n = (int)strlen(p.user);
        if (n > user) user = n;
        char tb[32];
        mtop_fmt_time(tb, sizeof(tb), p.total_cpu_ticks);
        n = (int)strlen(tb);
        if (n > time) time = n;
        n = snprintf(nullptr, 0, "%.1f", (double)p.cpu_pct);
        if (n > cpu) cpu = n;
        n = snprintf(nullptr, 0, "%.1f", (double)p.mem_pct);
        if (n > mem) mem = n;
    }
    return {pid, user, state, cpu, mem, time};
}

static void fmt_header_line(char* buf, size_t size, const MtopFmtWidth& w) {
    snprintf(buf, size,
        "%*s %-*s %*s %*s %*s %*s  %s",
        w.pid, "PID",
        w.user, "USER",
        w.state, "S",
        w.cpu, "%CPU",
        w.mem, "%MEM",
        w.time, "TIME+",
        "COMMAND");
}

static void fmt_proc_line(char* buf, size_t size, const MtopProcInfo& p, const MtopFmtWidth& w) {
    char timebuf[32];
    mtop_fmt_time(timebuf, sizeof(timebuf), p.total_cpu_ticks);

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

// ---------------------------------------------------------------------------
// MtopComponent
// ---------------------------------------------------------------------------

enum class SortMode { CPU, MEM, PID };

class MtopComponent : public ftxui::ComponentBase {
    std::vector<MtopProcInfo> procs_;
    MtopMemInfo mem_{};
    float uptime_ = 0;
    float loads_[3]{0};
    std::vector<MtopCpuInfo> cpu_stats_;
    std::vector<MtopCpuInfo> cpu_stats_prev_;
    std::unordered_map<unsigned, std::string> user_cache_;
    int only_pid_ = -1;
    int scroll_offset_ = 0;
    int max_rows_ = 0;
    int scroll_max_ = 0;
    SortMode sort_by_ = SortMode::CPU;
    MtopFmtWidth fmt_w_{5, 8, 1, 5, 5, 7};

public:
    MtopComponent(int only_pid) : only_pid_(only_pid) {}

    void Refresh() {
        mem_ = mtop_read_meminfo();
        uptime_ = mtop_read_uptime();
        mtop_read_loadavg(loads_);

        cpu_stats_prev_ = cpu_stats_;
        cpu_stats_ = mtop_read_cpu_stats();

        procs_ = mtop_read_procs(user_cache_, only_pid_);

        for (auto& p : procs_) {
            p.cpu_pct = mtop_calc_cpu_pct(p.total_cpu_ticks, uptime_,
                                           p.starttime);
            if (mem_.total > 0) {
                p.mem_pct =
                    100.0f * (float)(p.rss * mtop_page_sz / 1024) /
                    (float)mem_.total;
            }
        }

        // Sort by current mode
        switch (sort_by_) {
        case SortMode::CPU:
            std::sort(procs_.begin(), procs_.end(),
                      [](const MtopProcInfo& a, const MtopProcInfo& b) {
                          return a.cpu_pct > b.cpu_pct;
                      });
            break;
        case SortMode::MEM:
            std::sort(procs_.begin(), procs_.end(),
                      [](const MtopProcInfo& a, const MtopProcInfo& b) {
                          return a.mem_pct > b.mem_pct;
                      });
            break;
        case SortMode::PID:
            std::sort(procs_.begin(), procs_.end(),
                      [](const MtopProcInfo& a, const MtopProcInfo& b) {
                          return a.pid < b.pid;
                      });
            break;
        }

        fmt_w_ = calc_fmt_widths(procs_);

        if (auto* app = ftxui::App::Active()) {
            int h = app->dimy();
            int header_rows = 3 + (int)cpu_stats_.size() + 3;  // border+header+CPUs+border+mem+border
            max_rows_ = h - header_rows - 3;  // - border - footer
            if (max_rows_ < 1) max_rows_ = 1;
        }

        int total = (int)procs_.size();
        scroll_max_ = total - max_rows_;
        if (scroll_max_ < 0) scroll_max_ = 0;
        if (scroll_offset_ > scroll_max_) scroll_offset_ = scroll_max_;
    }

    ftxui::Element OnRender() override {
        using namespace ftxui;

        Elements sections;
        char buf[1024];

        // ---- Header ----
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

        // Btop-style header: green dot + hostname + uptime + load
        auto dot_el = text(" \xe2\x97\x8f ") | color(Color::Green) | bold;
        auto sys_el = text(hostname) | bold | color(Color::Cyan);
        snprintf(buf, sizeof(buf), "  up %d:%02d  ", hours, mins);
        auto uptime_el = text(buf) | color(Color::GrayLight);
        snprintf(buf, sizeof(buf), "load: %.2f %.2f %.2f",
                 loads_[0], loads_[1], loads_[2]);
        auto load_el = text(buf) | color(Color::GrayLight);
        snprintf(buf, sizeof(buf), "Tasks: %d [%d]", total, running);
        auto tasks_el = text(buf) | color(Color::GrayLight);

        sections.push_back(
            hbox({dot_el, sys_el, uptime_el, load_el, text("  "), tasks_el}) | flex_shrink);

        // ---- CPU bars ----
        Elements cpu_rows;
        for (int i = 0; i < (int)cpu_stats_.size() && i < 64; i++) {
            float pct = 0;
            if (i < (int)cpu_stats_prev_.size()) {
                unsigned long long total_now =
                    mtop_cpu_total(cpu_stats_[i]);
                unsigned long long total_prev =
                    mtop_cpu_total(cpu_stats_prev_[i]);
                unsigned long long idle_now = cpu_stats_[i].idle;
                unsigned long long idle_prev = cpu_stats_prev_[i].idle;
                unsigned long long dtotal = total_now - total_prev;
                unsigned long long didle = idle_now - idle_prev;
                if (dtotal > 0) {
                    pct = 100.0f * (float)(dtotal - didle) /
                          (float)dtotal;
                }
            }

            int bar_w = 30;
            int fill = (int)(pct * bar_w / 100.0f);
            if (fill > bar_w) fill = bar_w;
            if (fill < 0) fill = 0;

            std::string bar;
            make_bar(bar, bar_w, fill);

            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%5.1f%%", (double)pct);

            char label[16];
            if (mtop_ncpu > 1) {
                snprintf(label, sizeof(label), "CPU%-2d", i);
            } else {
                snprintf(label, sizeof(label), "CPU ");
            }

            auto bar_color = gradient_color(pct);
            auto row = hbox({
                text(label) | bold | color(Color::White),
                text(bar) | color(bar_color),
                text(pct_str) | color(Color::GrayLight),
            });
            cpu_rows.push_back(row);
        }
        sections.push_back(
            vbox(std::move(cpu_rows)) | border |
            color(Color::Default));

        // ---- Memory bar ----
        if (mem_.total > 0) {
            int bar_w = 30;
            unsigned long used = mem_.total - mem_.available;
            float used_pct =
                100.0f * (float)used / (float)mem_.total;
            int fill = (int)(used_pct * bar_w / 100.0f);
            if (fill > bar_w) fill = bar_w;
            if (fill < 0) fill = 0;

            std::string bar;
            make_bar(bar, bar_w, fill);

            char used_str[32], total_str[32];
            fmt_memsize(used_str, sizeof(used_str), used);
            fmt_memsize(total_str, sizeof(total_str), mem_.total);

            char mem_info[64];
            snprintf(mem_info, sizeof(mem_info), "%s / %s",
                     used_str, total_str);

            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%5.1f%%",
                     (double)used_pct);

            auto mem_color = gradient_color(used_pct);
            auto mem_row = hbox({
                text("MEM  ") | bold | color(Color::White),
                text(bar) | color(mem_color),
                text(pct_str) | color(Color::GrayLight),
                text(std::string("  ") + mem_info) |
                    color(Color::GrayLight),
            });
            sections.push_back(mem_row | border |
                               color(Color::Default));
        }

        // ---- Process list header ----
        char hdr[256];
        fmt_header_line(hdr, sizeof(hdr), fmt_w_);
        auto header_el = text(hdr) | bold | color(Color::Yellow);

        // ---- Process rows ----
        Elements proc_rows;
        proc_rows.push_back(separator());

        int display_count = (int)procs_.size();
        int avail = max_rows_;
        if (display_count > avail) display_count = avail;

        for (int i = 0; i < display_count; i++) {
            int idx = i + scroll_offset_;
            if (idx >= (int)procs_.size()) break;

            const auto& p = procs_[idx];

            char line[512];
            fmt_proc_line(line, sizeof(line), p, fmt_w_);

            auto el = text(line);
            // Color row by CPU usage
            if (p.cpu_pct >= 50.0f) {
                el = el | color(Color::Red);
            } else if (p.cpu_pct >= 10.0f) {
                el = el | color(Color::Yellow);
            } else if (p.cpu_pct >= 1.0f) {
                el = el | color(Color::Green);
            }
            // Alternating background
            if (i % 2 == 1) {
                el = el | bgcolor(Color::GrayDark);
            }
            proc_rows.push_back(el);
        }

        auto proc_panel = vbox({
            header_el,
            vbox(std::move(proc_rows)) | flex,
        });

        sections.push_back(proc_panel | border |
                           color(Color::Default));

        // ---- Footer ----
        {
            const char* sort_label = "CPU";
            if (sort_by_ == SortMode::MEM) sort_label = "MEM";
            else if (sort_by_ == SortMode::PID) sort_label = "PID";

            snprintf(buf, sizeof(buf),
                     " \xe2\x96\xb6 Sort: %s  "
                     "\xe2\x94\x82  q:quit  j\xe2\x86\x93k\xe2\x86\x91  "
                     "c:CPU  m:MEM  p:PID  "
                     "\xe2\x86\xb5Pg\xe2\x86\x91\xe2\x86\xb3Pg\xe2\x86\x93",
                     sort_label);
            sections.push_back(
                text(buf) | color(Color::GrayLight));
        }

        return vbox(std::move(sections));
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event == Event::Character('q') ||
            event == Event::Character('Q')) {
            if (auto* app = App::Active()) app->Exit();
            return true;
        }
        if (event == Event::Custom) {
            Refresh();
            return true;
        }
        if (event == Event::Character('j') ||
            event == Event::ArrowDown) {
            if (scroll_offset_ < scroll_max_) scroll_offset_++;
            return true;
        }
        if (event == Event::Character('k') ||
            event == Event::ArrowUp) {
            if (scroll_offset_ > 0) scroll_offset_--;
            return true;
        }
        if (event == Event::PageDown) {
            scroll_offset_ += max_rows_ / 2;
            if (scroll_offset_ > scroll_max_)
                scroll_offset_ = scroll_max_;
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
        // Sort toggles
        if (event == Event::Character('c') ||
            event == Event::Character('C')) {
            sort_by_ = SortMode::CPU;
            Refresh();
            return true;
        }
        if (event == Event::Character('m') ||
            event == Event::Character('M')) {
            sort_by_ = SortMode::MEM;
            Refresh();
            return true;
        }
        if (event == Event::Character('p') ||
            event == Event::Character('P')) {
            sort_by_ = SortMode::PID;
            Refresh();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }
};

// ---------------------------------------------------------------------------
// Public command entry point
// ---------------------------------------------------------------------------

void mtop_command(int argc, char** argv) {
    mtop_clk_tck = sysconf(_SC_CLK_TCK);
    mtop_page_sz = sysconf(_SC_PAGE_SIZE);
    if (mtop_clk_tck <= 0) mtop_clk_tck = 100;
    if (mtop_page_sz <= 0) mtop_page_sz = 4096;

    mtop_ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (mtop_ncpu < 1) mtop_ncpu = 1;

    struct arg_dbl* delay_opt =
        arg_dbl0("d", "delay", "SECS",
                 "delay between updates (default 1.0)");
    struct arg_int* pid_opt =
        arg_int0("p", "pid", "PID",
                 "monitor only this process");
    struct arg_lit* help_opt =
        arg_lit0("h", "help",
                 "display this help and exit");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {delay_opt, pid_opt, help_opt, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Interactive process viewer (Btop-style TUI).\n");
        printf("\n");
        printf("Options:\n");
        printf("  -d, --delay=SECS  "
               "delay between updates (default 1.0)\n");
        printf("  -p, --pid=PID     "
               "monitor only this process\n");
        printf("  -h, --help        "
               "display this help and exit\n");
        printf("\n");
        printf("Keys:\n");
        printf("  q         Quit\n");
        printf("  j / Down  Scroll down\n");
        printf("  k / Up    Scroll up\n");
        printf("  c         Sort by CPU\n");
        printf("  m         Sort by MEM\n");
        printf("  p         Sort by PID\n");
        printf("  PgUp      Scroll up half page\n");
        printf("  PgDn      Scroll down half page\n");
        printf("  Home      Scroll to top\n");
        printf("  End       Scroll to bottom\n");
        arg_freetable(argtable, sizeof(argtable) /
                                    sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) /
                                    sizeof(argtable[0]));
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

    arg_freetable(argtable,
                  sizeof(argtable) / sizeof(argtable[0]));

    auto screen = ftxui::App::Fullscreen();
    screen.TrackMouse(false);

    auto component = std::make_shared<MtopComponent>(only_pid);
    ftxui::Loop loop(&screen, component);

    mtop_read_cpu_stats();
    component->Refresh();

    std::atomic<bool> running{true};
    std::thread refresher([&screen, &running, delay]() {
        const auto step = std::chrono::milliseconds(50);
        const auto interval =
            std::chrono::milliseconds((int)(delay * 1000));
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

REGISTER_COMMAND("mtop", mtop_command, "Monitor processes (modern TUI)");
