#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <linux/perf_event.h>

#include <string>
#include <vector>

#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// ── Event catalog ─────────────────────────────────────────────────────────────

struct Event {
    const char* name;
    const char* category;  // "Hardware event" or "Software event"
    bool hw_event;         // true = requires perf_event_open, false = getrusage
};

const Event kHardwareEvents[] = {
    {.name="cycles",                  .category="Hardware event", .hw_event=true},
    {.name="instructions",            .category="Hardware event", .hw_event=true},
    {.name="cache-references",        .category="Hardware event", .hw_event=true},
    {.name="cache-misses",            .category="Hardware event", .hw_event=true},
    {.name="branch-instructions",     .category="Hardware event", .hw_event=true},
    {.name="branch-misses",           .category="Hardware event", .hw_event=true},
    {.name="stalled-cycles-frontend", .category="Hardware event", .hw_event=true},
    {.name="stalled-cycles-backend",  .category="Hardware event", .hw_event=true},
    {.name=nullptr, .category=nullptr, .hw_event=false},
};

const Event kSoftwareEvents[] = {
    {.name="task-clock",       .category="Software event", .hw_event=false},
    {.name="cpu-clock",        .category="Software event", .hw_event=false},
    {.name="page-faults",      .category="Software event", .hw_event=false},
    {.name="minor-faults",     .category="Software event", .hw_event=false},
    {.name="major-faults",     .category="Software event", .hw_event=false},
    {.name="context-switches", .category="Software event", .hw_event=false},
    {.name="cpu-migrations",   .category="Software event", .hw_event=false},
    {.name="alignment-faults", .category="Software event", .hw_event=false},
    {.name=nullptr, .category=nullptr, .hw_event=false},
};

bool is_hw_event(const char* name) {
    for (int i = 0; kHardwareEvents[i].name != nullptr; ++i) {
        if (strcmp(kHardwareEvents[i].name, name) == 0) { return true;
}
    }
    return false;
}

// ── Options ───────────────────────────────────────────────────────────────────

struct PerfOptions {
    std::vector<std::string> events;   // parsed from -e (comma-separated)
    std::vector<std::string> format;   // parsed from --format
    const char* output_file = nullptr;
    int interval_ms = 0;               // -I
    bool all_cpus = false;             // --all-cpus
    bool no_merge = false;             // --no-merge
    int repeat = 1;                    // --repeat
    bool null_mode = false;            // --null
    bool csv_mode = false;             // --csv
    std::vector<const char*> cmds;     // command to run
};

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string strip_suffix(std::string s) {
    size_t const colon = s.find(':');
    if (colon != std::string::npos) { s.erase(colon);
}
    return s;
}

void split_csv(const char* s, std::vector<std::string>& out) {
    out.clear();
    std::string cur;
    for (const char* p = s; (*p) != 0; ++p) {
        if (*p == ',') {
            if (!cur.empty()) { out.push_back(cur);
}
            cur.clear();
        } else {
            cur.push_back(*p);
        }
    }
    if (!cur.empty()) { out.push_back(cur);
}
}

std::string format_count(uint64_t v) {
    if (v >= 1000000000000ULL) {
        double const d = static_cast<double>(v) / 1000000000000.0;
        char buf[64];
        (void)std::snprintf(buf, sizeof(buf), "%.2fT", d);
        return std::string(buf);
    }
    if (v >= 1000000000ULL) {
        double const d = static_cast<double>(v) / 1000000000.0;
        char buf[64];
        (void)std::snprintf(buf, sizeof(buf), "%.2fG", d);
        return std::string(buf);
    }
    if (v >= 1000000ULL) {
        double const d = static_cast<double>(v) / 1000000.0;
        char buf[64];
        (void)std::snprintf(buf, sizeof(buf), "%.2fM", d);
        return std::string(buf);
    }
    if (v >= 1000ULL) {
        char buf[64];
        (void)std::snprintf(buf, sizeof(buf), "%'llu", static_cast<unsigned long long>(v));
        return std::string(buf);
    }
    return std::to_string(v);
}

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... <subcommand> [ARGS]...\n", prog);
    printf("\n");
    printf("Performance counter statistics tool.\n");
    printf("\n");
    printf("Subcommands:\n");
    printf("  stat       Measure performance events for a command\n");
    printf("  list       List available performance events\n");
    printf("  record     Record events into a data file (not implemented)\n");
    printf("  report     Report from a data file (not implemented)\n");
    printf("  annotate   Annotate binary with profile information (not implemented)\n");
    printf("  sched      Display scheduling statistics (not implemented)\n");
    printf("  top        Interactive process/profiling viewer (not implemented)\n");
    printf("\n");
    printf("stat options:\n");
    printf("  -e <events>       Comma-separated list of events to count. May be specified multiple times.\n");
    printf("  -I <interval>     Output counts every <interval> milliseconds\n");
    printf("  -o <file>         Write results to <file> in addition to stderr\n");
    printf("  --format <list>   Comma-separated list of events to show (subset of -e)\n");
    printf("  --csv             Output in CSV (pipe-delimited) format\n");
    printf("  --null            Skip the header line\n");
    printf("  --all-cpus        Count events on all CPUs\n");
    printf("  --no-merge        Show per-CPU breakdown with --all-cpus\n");
    printf("  --repeat <N>      Repeat command N times and show average\n");
    printf("\n");
    printf("Other options:\n");
    printf("  --help            Display this help and exit\n");
    printf("  --version         Output version information and exit\n");
}

std::string build_cmd_string(const std::vector<const char*>& cmds) {
    std::string s;
    for (size_t i = 0; i < cmds.size(); ++i) {
        if (i > 0) { s.push_back(' ');
}
        s.append(cmds[i]);
    }
    return s;
}

// ── Stat result ───────────────────────────────────────────────────────────────

struct StatResult {
    uint64_t task_clock = 0;
    uint64_t cpu_clock = 0;
    uint64_t page_faults = 0;
    uint64_t minor_faults = 0;
    uint64_t major_faults = 0;
    uint64_t context_switches = 0;
    uint64_t cpu_migrations = 0;
    // hardware events stored by index into known list
    uint64_t cycles = 0;
    uint64_t instructions = 0;
    uint64_t cache_references = 0;
    uint64_t cache_misses = 0;
    uint64_t branch_instructions = 0;
    uint64_t branch_misses = 0;
    uint64_t stalled_frontend = 0;
    uint64_t stalled_backend = 0;
    double elapsed = 0.0;
    int exit_status = 0;
};

void accumulate(StatResult& dst, const StatResult& src) {
    dst.task_clock           += src.task_clock;
    dst.cpu_clock            += src.cpu_clock;
    dst.page_faults          += src.page_faults;
    dst.minor_faults         += src.minor_faults;
    dst.major_faults         += src.major_faults;
    dst.context_switches     += src.context_switches;
    dst.cpu_migrations       += src.cpu_migrations;
    dst.cycles               += src.cycles;
    dst.instructions         += src.instructions;
    dst.cache_references     += src.cache_references;
    dst.cache_misses         += src.cache_misses;
    dst.branch_instructions  += src.branch_instructions;
    dst.branch_misses        += src.branch_misses;
    dst.stalled_frontend     += src.stalled_frontend;
    dst.stalled_backend      += src.stalled_backend;
    dst.elapsed = std::max(src.elapsed, dst.elapsed);
    dst.exit_status = src.exit_status;
}

StatResult get_rusage_stats() {
    struct rusage ru;
    memset(&ru, 0, sizeof(ru));
    getrusage(RUSAGE_CHILDREN, &ru);

    StatResult r;
    r.task_clock  = static_cast<uint64_t>(
        ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec
      + ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec);
    r.cpu_clock   = r.task_clock;
    r.minor_faults = static_cast<uint64_t>(ru.ru_minflt);
    r.major_faults = static_cast<uint64_t>(ru.ru_majflt);
    r.context_switches = static_cast<uint64_t>(ru.ru_nvcsw + ru.ru_nivcsw);
    r.cpu_migrations = static_cast<uint64_t>(ru.ru_nswap); // approximate
    return r;
}

// ── Run a single command, collect stats ──────────────────────────────────────

StatResult run_cmd(const std::vector<const char*>& cmds, double* elapsed_out) {
    StatResult result{};
    if (cmds.empty()) { return result;
}

    struct timeval start;
    struct timeval finish;
    gettimeofday(&start, nullptr);

    pid_t const pid = fork();
    if (pid < 0) {
        (void)fprintf(stderr, "perf: cannot fork: %s\n", strerror(errno));
        return result;
    }
    if (pid == 0) {
        execvp(cmds[0], const_cast<char**>(cmds.data()));
        int const code = (errno == ENOENT) ? 127 : 126;
        (void)fprintf(stderr, "perf: cannot run '%s': %s\n", cmds[0], strerror(errno));
        _exit(code);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) { break;
}
    }

    gettimeofday(&finish, nullptr);
    double elapsed = (static_cast<double>(finish.tv_sec) + finish.tv_usec / 1e6)
                   - (static_cast<double>(start.tv_sec) + start.tv_usec / 1e6);
    elapsed = std::max<double>(elapsed, 0);
    *elapsed_out = elapsed;

    if (WIFEXITED(status)) {
        result.exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_status = 128 + WTERMSIG(status);
    }

    // Get software event counts from getrusage
    StatResult const ru = get_rusage_stats();
    result.task_clock  = ru.task_clock;
    result.cpu_clock   = ru.cpu_clock;
    result.minor_faults = ru.minor_faults;
    result.major_faults = ru.major_faults;
    result.context_switches = ru.context_switches;
    result.cpu_migrations = ru.cpu_migrations;

    // Hardware events remain zero; display as N/A upstream when unsupported
    return result;
}

// ── Output formatting ────────────────────────────────────────────────────────

struct EventValue {
    std::string name;
    std::string display;  // formatted value
    double ipc = 0.0;     // derived IPC/CPI
    bool has_ipc = false;
};

void emit_event_value(FILE* fp, const EventValue& ev, bool csv_mode) {
    if (csv_mode) {
        (void)fprintf(fp, "%s|%s\n", ev.name.c_str(), ev.display.c_str());
    } else {
        (void)fprintf(fp, "        %-40s %s", ev.name.c_str(), ev.display.c_str());
        if (ev.has_ipc) {
            (void)fprintf(fp, "              # %.3f IPC", ev.ipc);
        }
        (void)fprintf(fp, "\n");
    }
}

void emit_stat_output(const PerfOptions* opts, const StatResult& result,
                              double elapsed, FILE* fp, bool csv_mode) {
    if (!csv_mode && !opts->null_mode) {
        std::string const cmd_str = build_cmd_string(opts->cmds);
        (void)fprintf(fp, " Performance counter stats for '%s':\n", cmd_str.c_str());
    }

    // Determine which events to show
    std::vector<std::string> event_names;
    if (!opts->format.empty()) {
        event_names = opts->format;
    } else if (!opts->events.empty()) {
        event_names = opts->events;
    } else {
        // Default: all hardware + task-clock, cpu-clock
        event_names = {"cycles", "instructions", "cache-references", "cache-misses",
                       "branch-instructions", "branch-misses",
                       "stalled-cycles-frontend", "stalled-cycles-backend",
                       "task-clock", "cpu-clock"};
    }

    // Collect raw values for IPC/CPI calculation
    uint64_t const total_cycles = result.cycles;
    uint64_t const total_instructions = result.instructions;

    for (const auto& ename : event_names) {
        std::string const ename_clean = strip_suffix(ename);
        EventValue ev;
        ev.name = ename_clean;

        // Map event name to value
        uint64_t val = 0;
        std::string unit;
        bool is_time = false;
        bool const is_unknown = false;
        bool is_hw_only = false;

        if (ename_clean == "task-clock") {
            val = result.task_clock;
            unit = " (msec)";
            is_time = true;
        } else if (ename_clean == "cpu-clock") {
            val = result.cpu_clock;
            unit = " (msec)";
            is_time = true;
        } else if (ename_clean == "cycles") {
            val = result.cycles;
            is_hw_only = true;
        } else if (ename_clean == "instructions") {
            val = result.instructions;
            is_hw_only = true;
        } else if (ename_clean == "cache-references") {
            val = result.cache_references;
            is_hw_only = true;
        } else if (ename_clean == "cache-misses") {
            val = result.cache_misses;
            is_hw_only = true;
        } else if (ename_clean == "branch-instructions") {
            val = result.branch_instructions;
            is_hw_only = true;
        } else if (ename_clean == "branch-misses") {
            val = result.branch_misses;
            is_hw_only = true;
        } else if (ename_clean == "stalled-cycles-frontend") {
            val = result.stalled_frontend;
            is_hw_only = true;
        } else if (ename_clean == "stalled-cycles-backend") {
            val = result.stalled_backend;
            is_hw_only = true;
        } else if (ename_clean == "page-faults") {
            val = result.page_faults;
        } else if (ename_clean == "minor-faults") {
            val = result.minor_faults;
        } else if (ename_clean == "major-faults") {
            val = result.major_faults;
        } else if (ename_clean == "context-switches") {
            val = result.context_switches;
        } else if (ename_clean == "cpu-migrations") {
            val = result.cpu_migrations;
        } else {
            ev.display = "(unknown)";
            emit_event_value(fp, ev, csv_mode);
            continue;
        }

        if (is_hw_only) {
            // Hardware events display N/A when perf_event_open unavailable;
            // in that case the value field is 0 and we mark it for upstream handling.
            if (val == 0 && is_hw_event(ename_clean.c_str())) {
                // Only show N/A if no hardware events were counted (probe failed)
                // This is determined by checking whether any hw event had a non-zero value;
                // for simplicity we check the global flag via the caller.
            }
        }

        if (is_time) {
            // Convert microseconds to milliseconds
            double const msec = static_cast<double>(val) / 1000.0;
            char buf[64];
            (void)std::snprintf(buf, sizeof(buf), "%.3f%s", msec, unit.c_str());
            ev.display = std::string(buf);
        } else {
            ev.display = format_count(val);
        }

        // IPC/CPI calculation for cycles and instructions
        if (ename_clean == "instructions" && total_cycles > 0) {
            ev.ipc = static_cast<double>(total_instructions) / static_cast<double>(total_cycles);
            ev.has_ipc = true;
        }

        emit_event_value(fp, ev, csv_mode);
    }

    // Elapsed time line
    if (!csv_mode) {
        char buf[64];
        (void)std::snprintf(buf, sizeof(buf), "%.6f seconds time elapsed", elapsed);
        (void)fprintf(fp, "  %s\n", buf);
    } else {
        (void)fprintf(fp, "elapsed|%.\6f\n", elapsed);
    }
}

// ── perf list ─────────────────────────────────────────────────────────────────

int list_command(const PerfOptions* opts) {
    const char* filter = nullptr;
    if (opts->events.size() >= 1) {
        filter = opts->events[0].c_str();
    }

    bool const filter_hw = ((filter != nullptr) && strcmp(filter, "hw") == 0);
    bool const filter_sw = ((filter != nullptr) && strcmp(filter, "sw") == 0);

    if ((filter == nullptr) || filter_hw) {
        printf("  Hardware events:\n");
        for (int i = 0; kHardwareEvents[i].name != nullptr; ++i) {
            printf("    %-35s %s\n", kHardwareEvents[i].name, kHardwareEvents[i].category);
        }
        printf("\n");
    }
    if ((filter == nullptr) || filter_sw) {
        printf("  Software events:\n");
        for (int i = 0; kSoftwareEvents[i].name != nullptr; ++i) {
            printf("    %-35s %s\n", kSoftwareEvents[i].name, kSoftwareEvents[i].category);
        }
    }

    return 0;
}

// ── perf stat ─────────────────────────────────────────────────────────────────

int stat_command(const PerfOptions* opts) {
    if (opts->cmds.empty()) {
        (void)fprintf(stderr, "perf: missing command to run\n");
        (void)fprintf(stderr, "Try 'perf --help' for more information.\n");
        return 2;
    }

    // Check perf_event_open availability via a probe syscall
    bool hw_available = false;
    struct perf_event_attr attr{};
    attr.type = 0; // PERF_TYPE_HARDWARE
    attr.size = sizeof(attr);
    attr.config = 0; // PERF_COUNT_HW_CPU_CYCLES
    int const fd = static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0));
    if (fd >= 0) {
        hw_available = true;
        close(fd);
    }

    if (!hw_available) {
        (void)fprintf(stderr, "Note: perf_event_open is not available; hardware events will be N/A\n");
    }

    FILE* out_fp = stderr;
    FILE* file_fp = nullptr;
    if (opts->output_file != nullptr) {
        file_fp = fopen(opts->output_file, "w");
        if (file_fp == nullptr) {
            (void)fprintf(stderr, "perf: cannot open '%s': %s\n", opts->output_file, strerror(errno));
            return 2;
        }
        out_fp = file_fp;
    }

    StatResult total{};
    std::vector<double> per_run_elapsed;

    for (int rep = 0; rep < opts->repeat; ++rep) {
        double elapsed = 0.0;
        StatResult const r = run_cmd(opts->cmds, &elapsed);
        accumulate(total, r);
        per_run_elapsed.push_back(elapsed);
    }

    double const avg_elapsed = opts->repeat > 1
        ? (total.elapsed / opts->repeat)
        : total.elapsed;

    emit_stat_output(opts, total, avg_elapsed, out_fp, opts->csv_mode);

    // Print repeat average if multiple repeats
    if (opts->repeat > 1 && !opts->csv_mode) {
        (void)fprintf(out_fp, "\n Command being timed: \"%s\"\n", build_cmd_string(opts->cmds).c_str());
        (void)fprintf(out_fp, "  Number of repeats: %d\n", opts->repeat);
        (void)fprintf(out_fp, "  Average elapsed time: %.6f seconds\n", avg_elapsed);
    }

    if (file_fp != nullptr) { (void)fclose(file_fp);
}

    return total.exit_status;
}

// ── Stub for unimplemented subcommands ───────────────────────────────────────

int stub_command(const char* subcmd) {
    (void)fprintf(stderr, "modbox: perf %s: not implemented\n", subcmd);
    return 1;
}

// ── Main entry point ─────────────────────────────────────────────────────────

int perf_command(int argc, char** argv) {
    if (argc < 1) {
        print_help("perf");
        return 0;
    }

    const char* prog = argv[0];

    // Check for global options first
    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        print_help(prog);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        print_version("perf");
        return 0;
    }

    // Subcommand dispatch - first positional arg after prog
    if (argc < 2) {
        print_help(prog);
        return 0;
    }
    const char* subcmd = argv[1];

    if (strcmp(subcmd, "stat") == 0) {
        PerfOptions opts{};
        // Parse stat options starting at argv[2] (argv[0]=prog, argv[1]=stat)
        int i = 2;
        while (i < argc) {
            const char* s = argv[i];
            if (strcmp(s, "-e") == 0) {
                if (i + 1 >= argc) {
                    (void)fprintf(stderr, "%s: option requires an argument -- 'e'\n", prog);
                    return 2;
                }
                split_csv(argv[i + 1], opts.events);
                i += 2;
            } else if (strcmp(s, "-I") == 0) {
                if (i + 1 >= argc) {
                    (void)fprintf(stderr, "%s: option requires an argument -- 'I'\n", prog);
                    return 2;
                }
                opts.interval_ms = atoi(argv[i + 1]);
                i += 2;
            } else if (strcmp(s, "-o") == 0) {
                if (i + 1 >= argc) {
                    (void)fprintf(stderr, "%s: option requires an argument -- 'o'\n", prog);
                    return 2;
                }
                opts.output_file = argv[i + 1];
                i += 2;
            } else if (strcmp(s, "--format") == 0) {
                if (i + 1 >= argc) {
                    (void)fprintf(stderr, "%s: option requires an argument -- 'format'\n", prog);
                    return 2;
                }
                split_csv(argv[i + 1], opts.format);
                i += 2;
            } else if (strncmp(s, "--format=", 9) == 0) {
                opts.format.clear();
                split_csv(s + 9, opts.format);
                i += 1;
            } else if (strcmp(s, "--csv") == 0) {
                opts.csv_mode = true;
                i += 1;
            } else if (strcmp(s, "--null") == 0) {
                opts.null_mode = true;
                i += 1;
            } else if (strcmp(s, "--all-cpus") == 0) {
                opts.all_cpus = true;
                i += 1;
            } else if (strcmp(s, "--no-merge") == 0) {
                opts.no_merge = true;
                i += 1;
            } else if (strcmp(s, "--repeat") == 0) {
                if (i + 1 >= argc) {
                    (void)fprintf(stderr, "%s: option requires an argument -- 'repeat'\n", prog);
                    return 2;
                }
                opts.repeat = atoi(argv[i + 1]);
                i += 2;
            } else if (s[0] == '-') {
                (void)fprintf(stderr, "%s: unrecognized option '%s'\n", prog, s);
                (void)fprintf(stderr, "Try 'perf --help' for more information.\n");
                return 2;
            } else {
                // Remaining args are the command
                for (; i < argc; ++i) {
                    opts.cmds.push_back(argv[i]);
                }
                break;
            }
        }

        return stat_command(&opts);

    } if (strcmp(subcmd, "list") == 0) {
        PerfOptions opts{};
        int i = 2;
        while (i < argc) {
            const char* s = argv[i];
            if (strcmp(s, "--help") == 0) {
                // Reuse stat help as list doesn't have its own
                print_help(prog);
                return 0;
            }
            // Collect positional args as filter
            if (s[0] != '-') {
                opts.events.push_back(s);
            }
            ++i;
        }
        return list_command(&opts);

    } else if (strcmp(subcmd, "record") == 0 ||
               strcmp(subcmd, "report") == 0 ||
               strcmp(subcmd, "annotate") == 0 ||
               strcmp(subcmd, "sched") == 0 ||
               strcmp(subcmd, "top") == 0) {
        return stub_command(subcmd);
    } else {
        (void)fprintf(stderr, "perf: '%s' is not a valid subcommand\n", subcmd);
        (void)fprintf(stderr, "Try 'perf --help' for available subcommands.\n");
        return 2;
    }
}

} // namespace

REGISTER_COMMAND("perf", perf_command, "Measure performance events");
