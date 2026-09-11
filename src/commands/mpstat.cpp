#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <sys/utsname.h>

#include "commands/mpstat.hpp"
#include "argtable3.h"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static std::string get_kernel_version() {
    struct utsname uts;
    uname(&uts);
    return std::string(uts.release);
}

static std::string get_hostname() {
    char buf[256] = {0};
    gethostname(buf, sizeof(buf));
    return std::string(buf);
}

static std::string get_arch() {
    struct utsname uts;
    uname(&uts);
    return std::string(uts.machine);
}

struct CpuTimes {
    long long user = 0;
    long long nice = 0;
    long long system = 0;
    long long idle = 0;
    long long iowait = 0;
    long long irq = 0;
    long long softirq = 0;
    long long steal = 0;
    long long guest = 0;
    long long guest_nice = 0;
};

static bool parse_cpu_line(const std::string& line, CpuTimes& t) {
    std::istringstream iss(line);
    std::string label;
    if (!(iss >> label)) { return false;
}
    if (!label.starts_with("cpu")) { return false;
}

    long long vals[10] = {0};
    for (int i = 0; i < 10; i++) {
        if (!(iss >> vals[i])) { return false;
}
    }

    t.user      = vals[0];
    t.nice      = vals[1];
    t.system    = vals[2];
    t.idle      = vals[3];
    t.iowait    = vals[4];
    t.irq       = vals[5];
    t.softirq   = vals[6];
    t.steal     = vals[7];
    t.guest     = vals[8];
    t.guest_nice = vals[9];
    return true;
}

static std::vector<CpuTimes> read_proc_stat() {
    std::vector<CpuTimes> result;
    std::ifstream f("/proc/stat");
    if (!f.is_open()) { return result;
}

    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with("cpu")) {
            CpuTimes t;
            if (parse_cpu_line(line, t)) {
                result.push_back(t);
            }
        }
    }
    return result;
}

static std::vector<std::string> read_cpu_labels() {
    std::vector<std::string> result;
    std::ifstream f("/proc/stat");
    if (!f.is_open()) { return result;
}

    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with("cpu")) {
            std::istringstream iss(line);
            std::string label;
            iss >> label;
            result.push_back(label);
        }
    }
    return result;
}

static void format_date_time(char* date_buf, size_t date_sz, char* time_buf, size_t time_sz) {
    time_t const now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    (void)strftime(date_buf, date_sz, "%Y-%m-%d", &tm_now);
    (void)strftime(time_buf, time_sz, "%H:%M:%S", &tm_now);
}

static std::string format_timestamp() {
    time_t const now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char buf[64];
    (void)strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_now);
    return std::string(buf);
}

static double safe_pct(long long num, long long den) {
    if (den == 0) { return 0.0;
}
    return (num * 100.0) / den;
}

struct StatEntry {
    std::string cpu;
    double usr = 0;
    double nice = 0;
    double sys = 0;
    double iowait = 0;
    double soft = 0;
    double irq = 0;
    double steal = 0;
    double guest = 0;
    double gnice = 0;
    double idle = 0;
};

static StatEntry compute_entry(const CpuTimes& prev, const CpuTimes& curr) {
    StatEntry e;

    long long diffs[10];
    diffs[0]  = curr.user      - prev.user;
    diffs[1]  = curr.nice      - prev.nice;
    diffs[2]  = curr.system    - prev.system;
    diffs[3]  = curr.idle      - prev.idle;
    diffs[4]  = curr.iowait    - prev.iowait;
    diffs[5]  = curr.irq       - prev.irq;
    diffs[6]  = curr.softirq   - prev.softirq;
    diffs[7]  = curr.steal     - prev.steal;
    diffs[8]  = curr.guest     - prev.guest;
    diffs[9]  = curr.guest_nice - prev.guest_nice;

    long long total = 0;
    for (int i = 0; i < 10; i++) { total += diffs[i];
}

    e.usr     = safe_pct(diffs[0], total);
    e.nice    = safe_pct(diffs[1], total);
    e.sys     = safe_pct(diffs[2], total);
    e.iowait  = safe_pct(diffs[4], total);
    e.soft    = safe_pct(diffs[6], total);
    e.irq     = safe_pct(diffs[5], total);
    e.steal   = safe_pct(diffs[7], total);
    e.guest   = safe_pct(diffs[8], total);
    e.gnice   = safe_pct(diffs[9], total);
    e.idle    = safe_pct(diffs[3], total);
    return e;
}

static bool is_per_cpu_label(const std::string& label) {
    if (label.size() < 4) { return false;
}
    return label.starts_with("cpu") && label[3] >= '0' && label[3] <= '9';
}

static std::string get_cpu_label(const std::string& label) {
    if (label == "cpu") { return "all";
}
    return label;
}

int mpstat_command(int argc, char** argv) {
    struct arg_lit* help_opt  = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* ver_opt   = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* json_opt  = arg_lit0("J", "json", "output as JSON");
    struct arg_lit* all_opt   = arg_lit0("a", "all", "report from all CPUs");
    struct arg_int* delay_opt = arg_int0(0, 0, "DELAY", "delay between updates");
    struct arg_int* count_opt = arg_int0(0, 0, "COUNT", "number of updates");
    struct arg_end* end       = arg_end(20);

    ArgTable at({help_opt, ver_opt, json_opt, all_opt, delay_opt, count_opt, end});

    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTIONS] [DELAY] [COUNT]\n", argv[0]);
        printf("\n");
        printf("Report CPU statistics.\n");
        printf("\n");
        printf("  -a, --all      report from all CPUs\n");
        printf("  -J, --json     output as JSON\n");
        printf("  -h, --help     display this help and exit\n");
        printf("  -V, --version  output version information and exit\n");
        return 0;
    }

    if (ver_opt->count > 0) {
        print_version("mpstat");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    int delay_sec = delay_opt->count > 0 ? delay_opt->ival[0] : 0;
    int count     = count_opt->count > 0 ? count_opt->ival[0] : 1;
    bool const json_mode = (json_opt->count > 0);
    bool const show_all  = (all_opt->count > 0);

    delay_sec = std::max(delay_sec, 0);
    count = std::max(count, 1);

    const long long US_PER_SEC = 1000000LL;

    std::string const kernel(get_kernel_version());
    std::string const host(get_hostname());
    std::string const arch(get_arch());

    char date_buf[32] = {0};
    char time_buf[32] = {0};
    format_date_time(date_buf, sizeof(date_buf), time_buf, sizeof(time_buf));

    auto labels = read_cpu_labels();
    auto samples = read_proc_stat();

    if (samples.empty()) {
        (void)fprintf(stderr, "mpstat: cannot read /proc/stat\n");
        return 1;
    }

    bool const has_aggregate = !labels.empty() && labels[0] == "cpu";

    std::ostringstream header_oss;
    header_oss << "Linux " << kernel << " (" << host << ")  "
               << arch << "  "
               << date_buf << "  " << time_buf
               << "  CPU  %usr  %nice  %sys  %iowait  %soft  %irq  %steal  %guest  %gnice  %idle";
    std::string const header = header_oss.str();

    if (json_mode) {
        std::vector<StatEntry> all_entries;

        for (int rep = 0; rep < count; rep++) {
            if (rep > 0 && delay_sec > 0) {
                usleep(static_cast<unsigned>(delay_sec) * US_PER_SEC);
            }

            auto cur_samples = read_proc_stat();
            auto cur_labels = read_cpu_labels();

            if (cur_samples.size() != samples.size() || cur_labels.size() != labels.size()) {
                samples = cur_samples;
                labels = cur_labels;
                continue;
            }

            for (size_t i = 0; i < samples.size(); i++) {
                if (!show_all && i != 0) { continue;
}
                StatEntry e = compute_entry(samples[i], cur_samples[i]);
                e.cpu = get_cpu_label(labels[i]);
                all_entries.push_back(e);
            }

            samples = cur_samples;
        }

        std::string const ts = format_timestamp();
        int const cpu_count = static_cast<int>(labels.size());

        printf("{\"timestamp\": \"%s\", \"cpu_count\": %d, \"statistics\": [", ts.c_str(), cpu_count);
        for (size_t i = 0; i < all_entries.size(); i++) {
            if (i > 0) { printf(", ");
}
            const auto& e = all_entries[i];
            printf("{\"cpu\": \"%s\", \"usr\": %.2f, \"nice\": %.2f, \"sys\": %.2f, \"iowait\": %.2f, \"soft\": %.2f, \"irq\": %.2f, \"steal\": %.2f, \"guest\": %.2f, \"gnice\": %.2f, \"idle\": %.2f}",
                   e.cpu.c_str(), e.usr, e.nice, e.sys, e.iowait, e.soft, e.irq, e.steal, e.guest, e.gnice, e.idle);
        }
        printf("]}\n");
    } else {
        printf("%s\n", header.c_str());

        for (int rep = 0; rep < count; rep++) {
            if (rep > 0 && delay_sec > 0) {
                usleep(static_cast<unsigned>(delay_sec) * US_PER_SEC);
            }

            auto cur_samples = read_proc_stat();

            if (cur_samples.size() != samples.size()) {
                samples = cur_samples;
                continue;
            }

            if (rep > 0 || delay_sec > 0) {
                for (size_t i = 0; i < samples.size(); i++) {
                    if (!show_all && i != 0) { continue;
}
                    if (!show_all && !has_aggregate) { continue;
}
                    StatEntry e = compute_entry(samples[i], cur_samples[i]);
                    e.cpu = get_cpu_label(labels[i]);
                    printf("%-9s %-8s %-5s %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %7.2f\n",
                           "", "", e.cpu.c_str(),
                           e.usr, e.nice, e.sys, e.iowait, e.soft, e.irq, e.steal, e.guest, e.gnice, e.idle);
                }
            } else {
                // First sample with no delay: all deltas are zero
                if (has_aggregate) {
                    printf("%-9s %-8s %-5s %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %7.2f\n",
                           "", "", "all", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
                }
                if (show_all) {
                    for (size_t i = 0; i < labels.size(); i++) {
                        if (!is_per_cpu_label(labels[i])) { continue;
}
                        printf("%-9s %-8s %-5s %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %5.2f %7.2f\n",
                               "", "", labels[i].c_str(),
                               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
                    }
                }
            }

            samples = cur_samples;
        }
    }

    return 0;
}

REGISTER_COMMAND("mpstat", mpstat_command, "Report CPU statistics");
