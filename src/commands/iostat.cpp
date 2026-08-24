#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cinttypes>
#include <unistd.h>

#include "commands/iostat.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Display CPU and I/O statistics.\n");
    printf("\n");
    printf("  -h, --help       display this help and exit\n");
    printf("  -V, --version    output version information and exit\n");
    printf("  -J, --json       output in JSON format\n");
    printf("  -S, --unit       byte scaling (K, M, G; default auto-sector)\n");
}

struct CpuStats {
    uint64_t user = 0;
    uint64_t nice = 0;
    uint64_t system = 0;
    uint64_t idle = 0;
    uint64_t iowait = 0;
    uint64_t irq = 0;
    uint64_t softirq = 0;
    uint64_t steal = 0;
    uint64_t guest = 0;
    uint64_t guest_nice = 0;
};

struct DiskStats {
    std::string name;
    uint64_t reads_completed = 0;
    uint64_t read_sectors = 0;
    uint64_t writes_completed = 0;
    uint64_t write_sectors = 0;
};

static uint64_t parse_u64(const char* s) {
    char* end = nullptr;
    uint64_t v = std::strtoull(s, &end, 10);
    if (end == s) return 0;
    return v;
}

static CpuStats read_cpu_stats() {
    CpuStats s{};
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return s;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // We only want the first "cpu " line (aggregate across all CPUs)
        if (strncmp(line, "cpu ", 4) == 0) {
            uint64_t vals[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            int n = sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                           &vals[0], &vals[1], &vals[2], &vals[3], &vals[4],
                           &vals[5], &vals[6], &vals[7], &vals[8], &vals[9]);
            if (n >= 8) {
                s.user = vals[0];
                s.nice = vals[1];
                s.system = vals[2];
                s.idle = vals[3];
                s.iowait = (n >= 5) ? vals[4] : 0;
                s.irq = (n >= 6) ? vals[5] : 0;
                s.softirq = (n >= 7) ? vals[6] : 0;
                s.steal = (n >= 8) ? vals[7] : 0;
            }
            if (n >= 9) s.guest = vals[8];
            if (n >= 10) s.guest_nice = vals[9];
            break;
        }
    }
    fclose(fp);
    return s;
}

static std::vector<DiskStats> read_disk_stats() {
    std::vector<DiskStats> result;
    FILE* fp = fopen("/proc/diskstats", "r");
    if (!fp) return result;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        unsigned maj, min;
        char name[64] = {0};
        uint64_t rcl, rse, wcl, wse;
        int n = sscanf(line, "%u %u %63s %" PRIu64 " %lu %" PRIu64 " %lu",
                       &maj, &min, name, &rcl, &rse, &wcl, &wse);
        if (n < 7 || name[0] == '\0') continue;
        // Skip partitions — keep only whole disks (look at name for common suffixes like p1, p2, etc.)
        // We include everything for simplicity since /proc/diskstats already filters well.
        DiskStats d{};
        d.name = name;
        d.reads_completed = rcl;
        d.read_sectors = rse;
        d.writes_completed = wcl;
        d.write_sectors = wse;
        result.push_back(std::move(d));
    }
    fclose(fp);
    return result;
}

static double get_uptime() {
    FILE* fp = fopen("/proc/uptime", "r");
    if (!fp) return 1.0;
    double u = 0.0;
    if (fscanf(fp, "%lf", &u) != 1) u = 1.0;
    fclose(fp);
    if (u <= 0) u = 1.0;
    return u;
}

// Compute delta between two snapshots. dt is in seconds.
// Returns (cpu_delta, disk_deltas).
static CpuStats cpu_delta(const CpuStats& old_s, const CpuStats& new_s) {
    CpuStats d{};
    d.user = new_s.user - old_s.user;
    d.nice = new_s.nice - old_s.nice;
    d.system = new_s.system - old_s.system;
    d.idle = new_s.idle - old_s.idle;
    d.iowait = new_s.iowait - old_s.iowait;
    d.irq = new_s.irq - old_s.irq;
    d.softirq = new_s.softirq - old_s.softirq;
    d.steal = new_s.steal - old_s.steal;
    d.guest = new_s.guest - old_s.guest;
    d.guest_nice = new_s.guest_nice - old_s.guest_nice;
    return d;
}

static std::vector<DiskStats> disk_delta(const std::vector<DiskStats>& old_s,
                                          const std::vector<DiskStats>& new_s,
                                          double dt) {
    (void)dt; // We compute per-disk rates below with own dt
    std::vector<DiskStats> deltas;
    // Build a map of old stats by name
    std::unordered_map<std::string, const DiskStats*> old_map;
    for (const auto& d : old_s) old_map[d.name] = &d;

    for (const auto& new_d : new_s) {
        auto it = old_map.find(new_d.name);
        DiskStats delta = new_d;
        if (it != old_map.end()) {
            const DiskStats& old_d = *it->second;
            delta.reads_completed -= old_d.reads_completed;
            delta.read_sectors -= old_d.read_sectors;
            delta.writes_completed -= old_d.writes_completed;
            delta.write_sectors -= old_d.write_sectors;
            // Clamp negative values (can happen with hot-plug)
            if (delta.reads_completed > new_d.reads_completed) delta.reads_completed = 0;
            if (delta.read_sectors > new_d.read_sectors) delta.read_sectors = 0;
            if (delta.writes_completed > new_d.writes_completed) delta.writes_completed = 0;
            if (delta.write_sectors > new_d.write_sectors) delta.write_sectors = 0;
        }
        deltas.push_back(std::move(delta));
    }
    return deltas;
}

static void emit_cpu_json(FILE* out, const CpuStats& delta, double dt) {
    uint64_t total = delta.user + delta.nice + delta.system + delta.idle
                   + delta.iowait + delta.irq + delta.softirq + delta.steal;
    double pct_user = dt > 0 ? 100.0 * (double)(delta.user) / total : 0.0;
    double pct_nice = dt > 0 ? 100.0 * (double)(delta.nice) / total : 0.0;
    double pct_system = dt > 0 ? 100.0 * (double)(delta.system) / total : 0.0;
    double pct_idle = dt > 0 ? 100.0 * (double)(delta.idle) / total : 0.0;
    double pct_iowait = dt > 0 ? 100.0 * (double)(delta.iowait) / total : 0.0;
    double pct_irq = dt > 0 ? 100.0 * (double)(delta.irq) / total : 0.0;
    double pct_softirq = dt > 0 ? 100.0 * (double)(delta.softirq) / total : 0.0;
    double pct_steal = dt > 0 ? 100.0 * (double)(delta.steal) / total : 0.0;

    (void)fprintf(out, "  \"cpu_statistics\": {\n");
    (void)fprintf(out, "    \"user\": %.1f,\n", pct_user);
    (void)fprintf(out, "    \"nice\": %.1f,\n", pct_nice);
    (void)fprintf(out, "    \"system\": %.1f,\n", pct_system);
    (void)fprintf(out, "    \"idle\": %.1f,\n", pct_idle);
    (void)fprintf(out, "    \"iowait\": %.1f,\n", pct_iowait);
    (void)fprintf(out, "    \"irq\": %.1f,\n", pct_irq);
    (void)fprintf(out, "    \"softirq\": %.1f,\n", pct_softirq);
    (void)fprintf(out, "    \"steal\": %.1f\n", pct_steal);
    (void)fprintf(out, "  },\n");
}

static void emit_table_header(FILE* out) {
    // "Linux 5.15.0 (hostname) \t %a %b %d %Y %H:%M:%S CST"
    char timestr[64];
    time_t now = time(nullptr);
    struct tm* tm_now = localtime(&now);
    strftime(timestr, sizeof(timestr), "%a %b %d %Y %H:%M:%S %Z", tm_now);
    printf("\nLinux %s \t %s\n", "unknown", timestr);
    printf("\n");
    printf("%-10s %8s %8s %8s %8s %8s\n",
           "Device", "tps", "rkB/s", "wkB/s", "areq-sz", "aqu-sz");
}

static void emit_table_row(FILE* out, const DiskStats& delta, const CpuStats& cpu_d, double dt) {
    if (dt <= 0) dt = 1.0;
    double tps = (double)(delta.reads_completed + delta.writes_completed) / dt;
    double rkB = (double)delta.read_sectors * 512.0 / dt / 1024.0;
    double wkB = (double)delta.write_sectors * 512.0 / dt / 1024.0;
    uint64_t total_sectors = delta.read_sectors + delta.write_sectors;
    uint64_t total_ops = delta.reads_completed + delta.writes_completed;
    double areq_sz = total_ops > 0 ? (double)total_sectors * 512.0 / (double)total_ops / 1024.0 : 0.0;
    double aqu_sz = tps > 0.001 ? (double)cpu_d.iowait / dt / 100.0 : 0.0;

    (void)fprintf(out, "%-10s %8.2f %8.2f %8.2f %8.2f %8.2f\n",
                  delta.name.c_str(), tps, rkB, wkB, areq_sz, aqu_sz);
}

static void emit_json_device(FILE* out, const DiskStats& delta, const CpuStats& cpu_d, double dt, bool last) {
    if (dt <= 0) dt = 1.0;
    double tps = (double)(delta.reads_completed + delta.writes_completed) / dt;
    double rkB = (double)delta.read_sectors * 512.0 / dt / 1024.0;
    double wkB = (double)delta.write_sectors * 512.0 / dt / 1024.0;
    uint64_t total_sectors = delta.read_sectors + delta.write_sectors;
    uint64_t total_ops = delta.reads_completed + delta.writes_completed;
    double areq_sz = total_ops > 0 ? (double)total_sectors * 512.0 / (double)total_ops / 1024.0 : 0.0;
    double aqu_sz = tps > 0.001 ? (double)cpu_d.iowait / dt / 100.0 : 0.0;

    (void)fprintf(out, "    {\"device\": ");
    json_escape_string(out, delta.name.c_str());
    (void)fprintf(out, ", \"tps\": %.2f, \"rkB/s\": %.2f, \"wkB/s\": %.2f, \"areq-sz\": %.2f, \"aqu-sz\": %.2f}",
                  tps, rkB, wkB, areq_sz, aqu_sz);
    if (!last) (void)fputc(',', out);
    (void)fputc('\n', out);
}

int iostat_command(int argc, char** argv) {
    bool json_mode = false;
    bool unit_flag = false;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            print_version("iostat");
            return 0;
        }
        if (strcmp(a, "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "-J") == 0 || strcmp(a, "--json") == 0) {
            json_mode = true;
            continue;
        }
        if (strcmp(a, "-S") == 0 || strcmp(a, "--unit") == 0) {
            if (a[2] != '\0') {
                unit_flag = true;
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                unit_flag = true;
                i++;
            }
            continue;
        }
        fprintf(stderr, "iostat: invalid option '%s'\n", argv[i]);
        fprintf(stderr, "Try 'iostat --help' for more information.\n");
        return 1;
    }

    // Single-shot mode: take two samples ~100ms apart for meaningful deltas
    CpuStats cpu0 = read_cpu_stats();
    auto disk0 = read_disk_stats();
    usleep(100000); // 100ms
    CpuStats cpu1 = read_cpu_stats();
    auto disk1 = read_disk_stats();
    double dt = 0.1;

    CpuStats cpu_d = cpu_delta(cpu0, cpu1);
    auto disk_d = disk_delta(disk0, disk1, dt);

    if (json_mode) {
        printf("{\n");
        emit_cpu_json(stdout, cpu_d, dt);
        printf("  \"device_stats\": [\n");
        for (size_t i = 0; i < disk_d.size(); i++) {
            emit_json_device(stdout, disk_d[i], cpu_d, dt, i + 1 == disk_d.size());
        }
        printf("  ]\n");
        printf("}\n");
    } else {
        emit_table_header(stdout);
        for (const auto& d : disk_d) {
            emit_table_row(stdout, d, cpu_d, dt);
        }
    }

    return 0;
}

REGISTER_COMMAND("iostat", iostat_command, "Display CPU and I/O statistics");
