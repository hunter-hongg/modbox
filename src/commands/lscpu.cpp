#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "commands/lscpu.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

static void print_help(const char* prog) {
    printf("Usage: lscpu [options]\n");
    printf("\n");
    printf("Display information about the CPU architecture.\n");
    printf("\n");
    printf("  -J, --json           use JSON output format\n");
    printf("  -e, --extended[=COLS] print out an extended readable format\n");
    printf("      --parse=<list>   use <list> of fields as output keys and values\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
}

struct CpuInfo {
    std::string architecture;
    std::string model_name;
    std::string vendor_id;
    int cpu_socket_count = 0;
    int cpu_core_per_socket = 0;
    int cpu_thread_per_core = 0;
    int cpu_online = 0;
    long long cpu_max_mhz = 0;
    long long cpu_min_mhz = 0;
    double bogomips = 0.0;
    std::string cache_l1d;
    std::string cache_l1i;
    std::string cache_l2;
    std::string cache_l3;
    std::vector<std::string> flags;
};

struct CpuTopologyEntry {
    int cpu;
    int core;
    int socket;
    int node;
};

static std::string trim_str(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    std::string result = ss.str();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
        result.pop_back();
    return result;
}

static bool safe_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long val = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    out = static_cast<int>(val);
    return true;
}

static bool safe_parse_long(const std::string& s, long long& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtoll(s.c_str(), &end, 10);
    return end != s.c_str();
}

static bool safe_parse_double(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end != s.c_str();
}

static CpuInfo parse_cpuinfo() {
    CpuInfo info;
    std::string content = read_file("/proc/cpuinfo");
    std::istringstream stream(content);
    std::string line;
    int processor_count = 0;
    int max_physical_id = 0;
    int max_core_id = 0;

    // Get architecture from uname
    struct utsname uts;
    uname(&uts);
    info.architecture = uts.machine;

    while (std::getline(stream, line)) {
        line = trim_str(line);
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        // Don't trim key yet - check for tabs before colon
        std::string key_raw = line.substr(0, colon);
        std::string value = trim_str(line.substr(colon + 1));

        // Remove trailing tabs from key
        while (!key_raw.empty() && key_raw.back() == '\t') {
            key_raw.pop_back();
        }
        std::string key = trim_str(key_raw);

        if (key == "processor") {
            processor_count++;
            continue;
        }

        // Track physical IDs and core IDs for topology
        if (key == "physical id") {
            int pid = 0;
            if (safe_parse_int(value, pid)) {
                if (pid > max_physical_id) max_physical_id = pid;
            }
        } else if (key == "core id") {
            int cid = 0;
            if (safe_parse_int(value, cid)) {
                if (cid > max_core_id) max_core_id = cid;
            }
        }

        // Only process first processor block for summary info
        if (processor_count <= 1) {
            if (key == "model name") {
                info.model_name = value;
            } else if (key == "vendor_id") {
                info.vendor_id = value;
            } else if (key == "bogomips") {
                safe_parse_double(value, info.bogomips);
            } else if (key == "flags") {
                std::istringstream iss(value);
                std::string flag;
                while (iss >> flag) info.flags.push_back(flag);
            } else if (key == "cache size") {
                info.cache_l2 = value;
            } else if (key == "cpu MHz") {
                safe_parse_long(value, info.cpu_max_mhz);
            }
        }
    }

    info.cpu_online = processor_count;
    info.cpu_socket_count = max_physical_id + 1;
    info.cpu_core_per_socket = max_core_id + 1;
    info.cpu_thread_per_core = processor_count / (max_physical_id + 1) / (max_core_id + 1);

    return info;
}

static void read_cache_info(CpuInfo& info) {
    const std::string cache_dir = "/sys/devices/system/cpu/cpu0/cache";
    
    for (int i = 0; i < 8; i++) {
        std::string type_path = cache_dir + "/index" + std::to_string(i) + "/type";
        std::string size_path = cache_dir + "/index" + std::to_string(i) + "/size";
        std::string level_path = cache_dir + "/index" + std::to_string(i) + "/level";
        
        std::string type = read_file(type_path);
        std::string size = read_file(size_path);
        std::string level_str = read_file(level_path);
        
        if (type.empty() || size.empty() || level_str.empty()) continue;
        
        int level = 0;
        if (!safe_parse_int(level_str, level)) continue;
        
        if (level == 1) {
            if (type == "Data") info.cache_l1d = size;
            else if (type == "Instruction") info.cache_l1i = size;
        } else if (level == 2) {
            info.cache_l2 = size;
        } else if (level == 3) {
            info.cache_l3 = size;
        }
    }
}

static void read_cpu_freq(CpuInfo& info) {
    const std::string freq_dir = "/sys/devices/system/cpu/cpu0/cpufreq";
    
    std::string min_freq = read_file(freq_dir + "/cpuinfo_min_freq");
    if (!min_freq.empty()) {
        long long freq_khz = 0;
        if (safe_parse_long(min_freq, freq_khz)) {
            info.cpu_min_mhz = freq_khz / 1000;
        }
    }
    
    std::string max_freq = read_file(freq_dir + "/cpuinfo_max_freq");
    if (!max_freq.empty()) {
        long long freq_khz = 0;
        if (safe_parse_long(max_freq, freq_khz)) {
            info.cpu_max_mhz = freq_khz / 1000;
        }
    }
}

static std::vector<CpuTopologyEntry> parse_cpu_topology() {
    std::vector<CpuTopologyEntry> result;
    const std::string syscpu = "/sys/devices/system/cpu";
    
    // Determine actual CPU count
    int max_cpu = 0;
    for (int cpu = 0; cpu < 1024; cpu++) {
        std::string cpu_dir = syscpu + "/cpu" + std::to_string(cpu);
        if (access(cpu_dir.c_str(), F_OK) != 0) {
            if (cpu > max_cpu) max_cpu = cpu;
            break;
        }
        max_cpu = cpu;
    }

    for (int cpu = 0; cpu <= max_cpu; cpu++) {
        std::string cpu_dir = syscpu + "/cpu" + std::to_string(cpu);
        std::string core_id_path = cpu_dir + "/topology/core_id";
        if (access(core_id_path.c_str(), F_OK) != 0) continue;

        int core_id = 0, socket_id = 0, numa = 0;
        safe_parse_int(read_file(cpu_dir + "/topology/core_id"), core_id);
        safe_parse_int(read_file(cpu_dir + "/topology/physical_package_id"), socket_id);
        {
            std::ifstream nf(cpu_dir + "/numa_node");
            if (nf.is_open()) nf >> numa;
        }

        result.push_back({cpu, core_id, socket_id, numa});
    }

    return result;
}

int lscpu_command(int argc, char** argv) {
    bool json_mode = false;
    bool extended = false;
    std::vector<std::string> parse_fields;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("lscpu");
            return 0;
        }
        if (strcmp(a, "-J") == 0 || strcmp(a, "--json") == 0) {
            json_mode = true;
            continue;
        }
        if (strcmp(a, "-e") == 0 || strcmp(a, "--extended") == 0) {
            extended = true;
            continue;
        }
        if (strncmp(a, "--extended", 10) == 0) {
            extended = true;
            continue;
        }
        if (strncmp(a, "--parse=", 8) == 0) {
            std::string fields = a + 8;
            size_t pos = 0;
            while (pos <= fields.size()) {
                size_t comma = fields.find(',', pos);
                if (comma == std::string::npos) comma = fields.size();
                std::string field = fields.substr(pos, comma - pos);
                // Trim
                size_t s = field.find_first_not_of(" \t");
                size_t e = field.find_last_not_of(" \t");
                if (s != std::string::npos) field = field.substr(s, e - s + 1);
                if (!field.empty()) parse_fields.push_back(field);
                pos = comma + 1;
            }
            continue;
        }
        if (a[0] == '-') {
            fprintf(stderr, "lscpu: unrecognized option '%s'\n", a);
            fprintf(stderr, "Try 'lscpu --help' for more information.\n");
            return 1;
        }
    }

    CpuInfo info = parse_cpuinfo();
    read_cache_info(info);
    read_cpu_freq(info);
    auto topology = parse_cpu_topology();

    if (!parse_fields.empty()) {
        // --parse= mode: output key=value pairs
        for (const auto& field : parse_fields) {
            std::string value;
            if (field == "Architecture") value = info.architecture;
            else if (field == "Model name") value = info.model_name;
            else if (field == "Vendor ID") value = info.vendor_id;
            else if (field == "CPU(s)") value = std::to_string(info.cpu_online);
            else if (field == "Thread(s) per core") value = std::to_string(info.cpu_thread_per_core);
            else if (field == "Core(s) per socket") value = std::to_string(info.cpu_core_per_socket);
            else if (field == "Socket(s)") value = std::to_string(info.cpu_socket_count);
            else if (field == "CPU max MHz") value = std::to_string(info.cpu_max_mhz);
            else if (field == "CPU min MHz") value = std::to_string(info.cpu_min_mhz);
            else if (field == "BogoMIPS") value = std::to_string(info.bogomips);
            else if (field == "L1d cache") value = info.cache_l1d;
            else if (field == "L1i cache") value = info.cache_l1i;
            else if (field == "L2 cache") value = info.cache_l2;
            else if (field == "L3 cache") value = info.cache_l3;
            else value = "<not found>";
            
            printf("%s=%s\n", field.c_str(), value.c_str());
        }
    } else if (json_mode) {
        printf("{\n");
        printf("  \"architecture\": "); json_escape_string(stdout, info.architecture.c_str()); printf(",\n");
        printf("  \"model_name\": "); json_escape_string(stdout, info.model_name.c_str()); printf(",\n");
        printf("  \"vendor_id\": "); json_escape_string(stdout, info.vendor_id.c_str()); printf(",\n");
        printf("  \"cpu_socket_count\": %d,\n", info.cpu_socket_count);
        printf("  \"cpu_core_per_socket\": %d,\n", info.cpu_core_per_socket);
        printf("  \"cpu_thread_per_core\": %d,\n", info.cpu_thread_per_core);
        printf("  \"cpu_online\": %d,\n", info.cpu_online);
        printf("  \"cpu_max_mhz\": %lld,\n", info.cpu_max_mhz);
        printf("  \"cpu_min_mhz\": %lld,\n", info.cpu_min_mhz);
        printf("  \"bogomips\": %.2f,\n", info.bogomips);
        printf("  \"cache\": {\n");
        printf("    \"l1d\": "); json_escape_string(stdout, info.cache_l1d.c_str()); printf(",\n");
        printf("    \"l1i\": "); json_escape_string(stdout, info.cache_l1i.c_str()); printf(",\n");
        printf("    \"l2\": "); json_escape_string(stdout, info.cache_l2.c_str()); printf(",\n");
        printf("    \"l3\": "); json_escape_string(stdout, info.cache_l3.c_str()); printf("\n");
        printf("  },\n");
        printf("  \"topology\": [\n");
        for (size_t i = 0; i < topology.size(); i++) {
            printf("    {\"cpu\": %d, \"core\": %d, \"socket\": %d, \"node\": %d}",
                   topology[i].cpu, topology[i].core, topology[i].socket, topology[i].node);
            if (i + 1 < topology.size()) printf(",");
            printf("\n");
        }
        printf("  ]\n");
        printf("}\n");
    } else if (extended) {
        printf("CPU CORE SOCKET NODE CACHE\n");
        for (const auto& t : topology) {
            printf("%d    %d    %d      %d    -\n", t.cpu, t.core, t.socket, t.node);
        }
    } else {
        printf("Architecture: %s\n", info.architecture.c_str());
        printf("CPU op-mode(s): 32-bit, 64-bit\n");
        printf("Byte Order: Little Endian\n");
        printf("CPU(s): %d\n", info.cpu_online);
        printf("On-line CPU(s) list: 0-%d\n", info.cpu_online - 1);
        printf("Vendor ID: %s\n", info.vendor_id.c_str());
        printf("Model name: %s\n", info.model_name.c_str());
        printf("Thread(s) per core: %d\n", info.cpu_thread_per_core);
        printf("Core(s) per socket: %d\n", info.cpu_core_per_socket);
        printf("Socket(s): %d\n", info.cpu_socket_count);
        if (info.cpu_max_mhz > 0 || info.cpu_min_mhz > 0) {
            printf("CPU max MHz: %lld\n", info.cpu_max_mhz);
            printf("CPU min MHz: %lld\n", info.cpu_min_mhz);
        }
        printf("BogoMIPS: %.2f\n", info.bogomips);
        if (!info.cache_l1d.empty()) printf("L1d cache: %s\n", info.cache_l1d.c_str());
        if (!info.cache_l1i.empty()) printf("L1i cache: %s\n", info.cache_l1i.c_str());
        if (!info.cache_l2.empty()) printf("L2 cache: %s\n", info.cache_l2.c_str());
        if (!info.cache_l3.empty()) printf("L3 cache: %s\n", info.cache_l3.c_str());
        if (!info.flags.empty()) {
            printf("Flags:");
            for (const auto& f : info.flags) printf(" %s", f.c_str());
            printf("\n");
        }
    }

    return 0;
}

REGISTER_COMMAND("lscpu", lscpu_command, "Display information about the CPU architecture");
