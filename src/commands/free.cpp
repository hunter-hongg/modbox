#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <argtable3.h>

#include "commands/free.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/json_stringifier.hpp"
#include "commands/cmd_error.hpp"

struct MemInfoData {
    int64_t mem_total = 0;
    int64_t mem_free = 0;
    int64_t mem_available = 0;
    int64_t swap_total = 0;
    int64_t swap_free = 0;
    int64_t buffers = 0;
    int64_t cached = 0;
    int64_t shmem = 0;
};

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

static MemInfoData read_proc_meminfo() {
    MemInfoData d{};
    auto content = read_file("/proc/meminfo");
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        std::istringstream lss(line);
        std::string key;
        long long val_kb = 0;
        lss >> key >> val_kb;
        if (key.empty()) continue;
        // Strip trailing colon from key
        if (!key.empty() && key.back() == ':') key.pop_back();

        if (key == "MemTotal") d.mem_total = val_kb;
        else if (key == "MemFree") d.mem_free = val_kb;
        else if (key == "MemAvailable") d.mem_available = val_kb;
        else if (key == "SwapTotal") d.swap_total = val_kb;
        else if (key == "SwapFree") d.swap_free = val_kb;
        else if (key == "Buffers") d.buffers = val_kb;
        else if (key == "Cached") d.cached = val_kb;
        else if (key == "Shmem") d.shmem = val_kb;
    }

    return d;
}

static void print_help(const char* prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("\nOptions:\n");
    printf("  -h, --human-readable  print sizes in human-readable format\n");
    printf("      --si              use powers of 1000 not 1024\n");
    printf("  -t, --total           show total row\n");
    printf("  -o, --old             use the old (legacy) format\n");
    printf("      --json            output in JSON format\n");
    printf("  -h, --help            display this help and exit\n");
    printf("  -V, --version         output version information and exit\n");
}

static void print_version() {
    printf("free (modbox) 1.0\n");
}

// Human-readable formatting: 1024-based with KiB/MiB/GiB suffixes
static void format_human(FILE* out, int64_t val_kb) {
    int64_t unit = 1024;
    if (val_kb < unit) {
        fprintf(out, "%lldB", val_kb);
        return;
    }
    double v = (double)val_kb;
    const char* suffixes[] = {"K", "M", "G", "T", "P", "E", "Z", "Y"};
    int idx = 0;
    while (v >= (double)unit && idx < 7) {
        v /= (double)unit;
        idx++;
    }
    fprintf(out, "%.1f%s", v, suffixes[idx]);
}

// SI formatting: 1000-based with kB/MB/GB suffixes
static void format_si(FILE* out, int64_t val_kb) {
    int64_t unit = 1000;
    if (val_kb < unit) {
        fprintf(out, "%llukB", val_kb);
        return;
    }
    double v = (double)val_kb;
    const char* suffixes[] = {"kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    int idx = 0;
    while (v >= (double)unit && idx < 7) {
        v /= (double)unit;
        idx++;
    }
    fprintf(out, "%.1f%s", v, suffixes[idx]);
}

// Print a value in the appropriate format
static void print_value(FILE* out, int64_t val_kb, bool human, bool si) {
    if (human || si) {
        if (si) format_si(out, val_kb);
        else format_human(out, val_kb);
    } else {
        fprintf(out, "%llu", (unsigned long long)val_kb);
    }
}

// Print the header line
static void print_header(bool old_format) {
    printf("              ");
    printf("%-10s", "total");
    printf("%-10s", "used");
    printf("%-10s", "free");
    printf("%-10s", "shared");
    printf("%-10s", "buff/cache");
    if (!old_format) {
        printf("%-10s", "available");
    }
    printf("\n");
}

// Print a data row with the label prefix
static void print_row(const char* label, int64_t total, int64_t used,
                      int64_t free_mem, int64_t shared, int64_t buff_cache,
                      int64_t available, bool human, bool si, bool old_format) {
    printf("%-13s", label);
    print_value(stdout, total, human, si);
    printf("%13s", "");
    print_value(stdout, used, human, si);
    printf("%13s", "");
    print_value(stdout, free_mem, human, si);
    printf("%13s", "");
    print_value(stdout, shared, human, si);
    printf("%13s", "");
    print_value(stdout, buff_cache, human, si);
    if (!old_format) {
        printf("%13s", "");
        print_value(stdout, available, human, si);
    }
    printf("\n");
}

int free_command(int argc, char** argv) {
    // -h maps to human-readable; -V for version (GNU free uses --version)
    struct arg_lit* human_opt = arg_lit0("h", "human-readable", "print sizes in human-readable format");
    struct arg_lit* si_opt = arg_lit0(NULL, "si", "use powers of 1000 not 1024");
    struct arg_lit* total_opt = arg_lit0("t", "total", "show total row");
    struct arg_lit* old_opt = arg_lit0("o", "old", "use the old (legacy) format");
    struct arg_lit* json_opt = arg_lit0(NULL, "json", "output in JSON format");
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_end* end = arg_end(20);

    ArgTable at({human_opt, si_opt, total_opt, old_opt, json_opt, help_opt, version_opt, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        print_help(argv[0]);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version();
        return 0;
    }

    if (end->count > 0 || nerrors > 0) {
        at.print_errors(end, argv[0]);
        return 1;
    }
    bool human = (human_opt->count > 0);
    bool si = (si_opt->count > 0);
    bool show_total = (total_opt->count > 0);
    bool old_format = (old_opt->count > 0);
    bool json_mode = (json_opt->count > 0);

    // Check for positional arguments
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            cmd_error(argv[0], "unexpected argument \"%s\"", argv[i]);
            return 1;
        }
    }

    MemInfoData mem = read_proc_meminfo();

    // Compute derived values
    int64_t mem_used = mem.mem_total - mem.mem_free;
    int64_t buff_cache = mem.buffers + mem.cached;
    int64_t shared = mem.shmem;
    int64_t swap_used = mem.swap_total - mem.swap_free;

    if (json_mode) {
        fprintf(stdout, "{\n");
        fprintf(stdout, "  \"mem\": {\n");
        json_emit_long(stdout, "available", mem.mem_available, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "buff_cache", buff_cache, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "free", mem.mem_free, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "shared", shared, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "total", mem.mem_total, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "used", mem_used, true);
        fprintf(stdout, "\n");
        fprintf(stdout, "  },\n");
        fprintf(stdout, "  \"swap\": {\n");
        json_emit_long(stdout, "free", mem.swap_free, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "total", mem.swap_total, false);
        fprintf(stdout, ",\n");
        json_emit_long(stdout, "used", swap_used, true);
        fprintf(stdout, "\n");
        fprintf(stdout, "  }\n");
        fprintf(stdout, "}\n");
        return 0;
    }

    // Text output
    print_header(old_format);

    print_row("Mem:", mem.mem_total, mem_used, mem.mem_free, shared,
              buff_cache, mem.mem_available, human, si, old_format);

    print_row("Swap:", mem.swap_total, swap_used, mem.swap_free, 0, 0, 0,
              human, si, old_format);

    if (show_total) {
        uint64_t grand_total = mem.mem_total + mem.swap_total;
        uint64_t grand_used = mem_used + swap_used;
        uint64_t grand_free = mem.mem_free + mem.swap_free;
        print_row("total:", grand_total, grand_used, grand_free, shared,
                  buff_cache, mem.mem_available, human, si, old_format);
    }

    return 0;
}

REGISTER_COMMAND("free", free_command, "Display amount of free and used memory in the system");
