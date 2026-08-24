#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <argtable3.h>

#include "commands/vmstat.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

struct ProcStatData {
    long long user = 0, nice = 0, system = 0, idle = 0;
    long long iowait = 0, irq = 0, softirq = 0, steal = 0;
    long long pgfault = 0;
    long long ctxt = 0;
    long long procs_running = 0;
    long long procs_blocked = 0;
    long long pswpin = 0;
    long long pswpout = 0;
};

struct ProcMeminfoData {
    long long mem_total = 0, mem_free = 0, mem_available = 0;
    long long active = 0, inactive = 0;
    long long swap_total = 0, swap_free = 0;
    long long buffers = 0, cached = 0, slab = 0;
};

struct ProcDiskstatEntry {
    std::string name;
    long long reads_completed = 0;
    long long reads_merged = 0;
    long long sectors_read = 0;
    long long time_reading_ms = 0;
    long long writes_completed = 0;
    long long writes_merged = 0;
    long long sectors_written = 0;
    long long time_writing_ms = 0;
    long long io_in_progress_ms = 0;
    long long io_time_ms = 0;
    long long io_weighted_ms = 0;
};

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static ProcStatData read_proc_stat() {
    ProcStatData d{};
    auto content = read_file("/proc/stat");
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.rfind("cpu ", 0) == 0) {
            std::istringstream lss(line);
            std::string label;
            lss >> label;
            lss >> d.user >> d.nice >> d.system >> d.idle >> d.iowait
                >> d.irq >> d.softirq >> d.steal;
            break;
        }
    }

    while (std::getline(iss, line)) {
        if (line.rfind("pgfault", 0) == 0) {
            std::istringstream lss(line);
            std::string label;
            lss >> label >> d.pgfault;
            break;
        }
    }

    while (std::getline(iss, line)) {
        if (line.rfind("ctxt", 0) == 0) {
            std::istringstream lss(line);
            std::string label;
            lss >> label >> d.ctxt;
            break;
        }
    }

    while (std::getline(iss, line)) {
        std::istringstream lss(line);
        std::string label;
        lss >> label;
        if (label == "procs_running") lss >> d.procs_running;
        else if (label == "procs_blocked") lss >> d.procs_blocked;
        else if (label == "pswpin") lss >> d.pswpin;
        else if (label == "pswpout") lss >> d.pswpout;
    }

    return d;
}

static ProcMeminfoData read_proc_meminfo() {
    ProcMeminfoData d{};
    auto content = read_file("/proc/meminfo");
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        std::istringstream lss(line);
        std::string key;
        long long val_kb = 0;
        std::string unit;
        lss >> key >> val_kb;
        lss >> unit;

        if (key == "MemTotal:") d.mem_total = val_kb;
        else if (key == "MemFree:") d.mem_free = val_kb;
        else if (key == "MemAvailable:") d.mem_available = val_kb;
        else if (key == "Active:") d.active = val_kb;
        else if (key == "Inactive:") d.inactive = val_kb;
        else if (key == "SwapTotal:") d.swap_total = val_kb;
        else if (key == "SwapFree:") d.swap_free = val_kb;
        else if (key == "Buffers:") d.buffers = val_kb;
        else if (key == "Cached:") d.cached = val_kb;
        else if (key == "Slab:") d.slab = val_kb;
    }

    return d;
}

static std::vector<ProcDiskstatEntry> read_proc_diskstats() {
    std::vector<ProcDiskstatEntry> entries;
    auto content = read_file("/proc/diskstats");
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        std::istringstream lss(line);
        ProcDiskstatEntry e;
        int major = 0, minor = 0;
        std::string name_str;
        lss >> major >> minor >> e.name >> e.reads_completed >> e.reads_merged
            >> e.sectors_read >> e.time_reading_ms >> e.writes_completed
            >> e.writes_merged >> e.sectors_written >> e.time_writing_ms
            >> e.io_in_progress_ms >> e.io_time_ms >> e.io_weighted_ms;
        if (!e.name.empty()) {
            entries.push_back(std::move(e));
        }
    }

    return entries;
}

static double read_proc_uptime() {
    auto content = read_file("/proc/uptime");
    if (content.empty()) return 0.0;
    double uptime_secs = 0.0;
    std::istringstream iss(content);
    iss >> uptime_secs;
    return uptime_secs;
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS] [delay [count]]\n", prog);
    printf("\nOptions:\n");
    printf("  -a          display active/inactive memory fields\n");
    printf("  -d          display disk statistics\n");
    printf("  -h, --help  display this help and exit\n");
    printf("  -V, --version  output version information and exit\n");
    printf("\nIf no delay is specified, outputs a single summary of accumulated\n");
    printf("statistics since system startup. If delay is specified, vmstat loops\n");
    printf("once per delay seconds printing each time.\n");
}

static void print_version() {
    printf("vmstat (modbox) 1.0\n");
}

static int do_snapshot(bool show_active, bool show_disk) {
    ProcStatData stat = read_proc_stat();
    ProcMeminfoData mem = read_proc_meminfo();

    if (show_active) {
        printf("procs  memory                                     swap              io          system-------cpu------\n");
        printf("r      b          swpd    free    inact   active     si      so      bi      bo      in      cs      us      sy      id\n");
    } else {
        printf("procs  memory                                     swap              io          system-------cpu------\n");
        printf("r      b          swpd    free    buff    cache      si      so      bi      bo      in      cs      us      sy      id\n");
    }

    if (!show_active) {
        printf("%-7lld%-7lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld\n",
               (long long)stat.procs_running,
               (long long)stat.procs_blocked,
               (long long)(mem.swap_total - mem.swap_free),
               (long long)mem.mem_free / 1024,
               (long long)mem.buffers / 1024,
               (long long)mem.cached / 1024,
               0LL, 0LL, 0LL, 0LL,
               (long long)stat.ctxt,
               (long long)stat.pgfault,
               0LL, 0LL, 0LL);
    } else {
        printf("%-7lld%-7lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld%8lld\n",
               (long long)stat.procs_running,
               (long long)stat.procs_blocked,
               (long long)(mem.swap_total - mem.swap_free),
               (long long)mem.mem_free / 1024,
               (long long)mem.inactive / 1024,
               (long long)mem.active / 1024,
               0LL, 0LL, 0LL, 0LL,
               (long long)stat.ctxt,
               (long long)stat.pgfault,
               0LL, 0LL, 0LL);
    }

    if (show_disk) {
        printf("\n Disk statistics\n");
        printf("%-16s%8s%8s\n", "name", "read", "write");
        auto disks = read_proc_diskstats();
        for (const auto& disk : disks) {
            printf("%-16s%8lld%8lld\n", disk.name.c_str(),
                   disk.reads_completed, disk.writes_completed);
        }
    }

    return 0;
}

int vmstat_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_lit* active_opt = arg_lit0("a", nullptr, "show active/inactive memory");
    struct arg_lit* disk_opt = arg_lit0("d", nullptr, "show disk statistics");
    struct arg_int* delay_opt = arg_intn(nullptr, nullptr, "DELAY", 0, 1, "iteration delay");
    struct arg_int* count_opt = arg_intn(nullptr, nullptr, "COUNT", 0, 1, "number of updates");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, active_opt, disk_opt, delay_opt, count_opt, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version();
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        return 1;
    }

    bool show_active = (active_opt->count > 0);
    bool show_disk = (disk_opt->count > 0);
    int delay_sec = 0;
    int count = 1;

    if (delay_opt->count > 0 && delay_opt->ival[0] >= 0) {
        delay_sec = delay_opt->ival[0];
    }

    if (count_opt->count > 0 && count_opt->ival[0] > 0) {
        count = count_opt->ival[0];
    }

    if (delay_sec == 0 && count > 1) {
        fprintf(stderr, "vmstat: delay required when count is specified\n");
        return 1;
    }

    if (delay_sec == 0) {
        do_snapshot(show_active, show_disk);
    } else {
        for (int i = 0; i < count || count == 1; ++i) {
            do_snapshot(show_active, show_disk);
            if (i < count - 1) {
                std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
            }
        }
    }

    return 0;
}

REGISTER_COMMAND("vmstat", vmstat_command, "Display system performance statistics");
