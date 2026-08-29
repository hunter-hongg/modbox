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

struct FreeOptions {
    bool human = false;
    bool si = false;
    bool show_total = false;
    bool old_format = false;
    bool json_mode = false;
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
        int64_t val_kb = 0;
        lss >> key >> val_kb;
        if (key.empty()) continue;
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
    printf("      --help            display this help and exit\n");
    printf("      --version         output version information and exit\n");
}

static void print_version() {
    printf("free (modbox) 1.0\n");
}

static void format_size(FILE* out, int64_t val_kb, bool si) {
    int64_t unit = si ? 1000 : 1024;
    if (val_kb < unit) {
        fprintf(out, "%lldB", val_kb);
        return;
    }
    double v = (double)val_kb;
    const char* suffixes = si ? "kMGTPEB" : "KMGTPEZY";
    int idx = 0;
    while (v >= (double)unit && idx < 7) {
        v /= (double)unit;
        idx++;
    }
    if (si) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f%cB", v, suffixes[idx]);
        fprintf(out, "%s", buf);
    } else {
        fprintf(out, "%.1f%c", v, suffixes[idx]);
    }
}

static void print_value(FILE* out, int64_t val_kb, const FreeOptions* opts) {
    if (opts->human || opts->si) {
        format_size(out, val_kb, opts->si);
    } else {
        fprintf(out, "%lld", val_kb);
    }
}

static void print_header(const FreeOptions* opts) {
    printf("              ");
    printf("%-10s", "total");
    printf("%-10s", "used");
    printf("%-10s", "free");
    printf("%-10s", "shared");
    printf("%-10s", "buff/cache");
    if (!opts->old_format) {
        printf("%-10s", "available");
    }
    printf("\n");
}

static void print_row(const char* label, int64_t total, int64_t used,
                      int64_t free_mem, int64_t shared, int64_t buff_cache,
                      int64_t available, const FreeOptions* opts) {
    printf("%-13s", label);
    print_value(stdout, total, opts);
    printf("%13s", "");
    print_value(stdout, used, opts);
    printf("%13s", "");
    print_value(stdout, free_mem, opts);
    printf("%13s", "");
    print_value(stdout, shared, opts);
    printf("%13s", "");
    print_value(stdout, buff_cache, opts);
    if (!opts->old_format) {
        printf("%13s", "");
        print_value(stdout, available, opts);
    }
    printf("\n");
}

int free_command(int argc, char** argv) {
    struct arg_lit* human_opt = arg_lit0("h", "human-readable", "print sizes in human-readable format");
    struct arg_lit* si_opt = arg_lit0(NULL, "si", "use powers of 1000 not 1024");
    struct arg_lit* total_opt = arg_lit0("t", "total", "show total row");
    struct arg_lit* old_opt = arg_lit0("o", "old", "use the old (legacy) format");
    struct arg_lit* json_opt = arg_lit0(NULL, "json", "output in JSON format");
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
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

    FreeOptions opts;
    opts.human = (human_opt->count > 0);
    opts.si = (si_opt->count > 0);
    opts.show_total = (total_opt->count > 0);
    opts.old_format = (old_opt->count > 0);
    opts.json_mode = (json_opt->count > 0);

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            cmd_error(argv[0], "unexpected argument \"%s\"", argv[i]);
            return 1;
        }
    }

    MemInfoData mem = read_proc_meminfo();

    int64_t buff_cache = mem.buffers + mem.cached;
    int64_t shared = mem.shmem;
    int64_t mem_used = mem.mem_total - mem.mem_free - buff_cache - shared;
    int64_t swap_used = mem.swap_total - mem.swap_free;

    if (opts.json_mode) {
        fprintf(stdout, "{\n");
        fprintf(stdout, "  \"mem\": {\n");
        fprintf(stdout, "    \"available\": %lld,\n", mem.mem_available);
        fprintf(stdout, "    \"buff_cache\": %lld,\n", buff_cache);
        fprintf(stdout, "    \"free\": %lld,\n", mem.mem_free);
        fprintf(stdout, "    \"shared\": %lld,\n", shared);
        fprintf(stdout, "    \"total\": %lld,\n", mem.mem_total);
        fprintf(stdout, "    \"used\": %lld\n", mem_used);
        fprintf(stdout, "  },\n");
        fprintf(stdout, "  \"swap\": {\n");
        fprintf(stdout, "    \"free\": %lld,\n", mem.swap_free);
        fprintf(stdout, "    \"total\": %lld,\n", mem.swap_total);
        fprintf(stdout, "    \"used\": %lld\n", swap_used);
        fprintf(stdout, "  }\n");
        fprintf(stdout, "}\n");
        return 0;
    }

    print_header(&opts);

    print_row("Mem:", mem.mem_total, mem_used, mem.mem_free, shared,
              buff_cache, mem.mem_available, &opts);

    print_row("Swap:", mem.swap_total, swap_used, mem.swap_free, 0, 0, 0, &opts);

    if (opts.show_total) {
        int64_t grand_total = mem.mem_total + mem.swap_total;
        int64_t grand_used = mem_used + swap_used;
        int64_t grand_free = mem.mem_free + mem.swap_free;
        print_row("total:", grand_total, grand_used, grand_free, 0, 0, 0, &opts);
    }

    return 0;
}

REGISTER_COMMAND("free", free_command, "Display amount of free and used memory in the system");
