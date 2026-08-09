#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "commands/lsblk.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/json_stringifier.hpp"

static void print_help(const char* prog) {
    printf("Usage: lsblk [options] [<device> ...]\n");
    printf("\n");
    printf("List information about block devices.\n");
    printf("\n");
    printf("  -a, --all            print all devices including empty ones\n");
    printf("  -b, --bytes          print SIZE in bytes\n");
    printf("  -J, --json           use JSON output format\n");
    printf("  -n, --noheadings     don't print headings\n");
    printf("  -o, --output <list>  output columns\n");
    printf("  -p, --paths          print complete device paths\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
}

struct BlockDevice {
    std::string name;
    std::string maj_min;
    bool removable = false;
    long long size_bytes = 0;
    std::string type = "disk";
    std::string mountpoint;
    std::string fs_type;
    std::vector<std::string> children;
};

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string read_sysfs(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return "";
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    // Strip trailing whitespace/newlines
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t'))
        buf[--n] = '\0';
    return std::string(buf);
}

static std::vector<BlockDevice> collect_devices(bool include_all) {
    std::vector<BlockDevice> devices;
    const std::string block_dir = "/sys/block";
    std::vector<std::string> dev_names;


    DIR* dir = opendir(block_dir.c_str());
    if (!dir) return devices;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string dname = entry->d_name;
        if (!include_all && dname.find("loop") == 0) continue;
        dev_names.push_back(dname);
    }
    closedir(dir);
    std::sort(dev_names.begin(), dev_names.end());

    for (const auto& dname : dev_names) {
        BlockDevice dev;
        dev.name = dname;

        // Read size (sectors)
        std::string size_str = read_sysfs(block_dir + "/" + dname + "/size");
        if (!size_str.empty()) {
            dev.size_bytes = std::strtoll(size_str.c_str(), nullptr, 10) * 512;
        }

        // Read removable
        std::string rm_str = read_sysfs(block_dir + "/" + dname + "/removable");
        if (!rm_str.empty()) {
            dev.removable = (std::atoi(rm_str.c_str()) != 0);
        }

        // Read maj:min
        std::string majmin_str = read_sysfs(block_dir + "/" + dname + "/dev");
        if (!majmin_str.empty()) {
            dev.maj_min = majmin_str;
        }

        // Read mountpoints from /proc/mounts
        FILE* mounts = fopen("/proc/mounts", "r");
        if (mounts) {
            char line[512];
            std::string devpath = "/dev/" + dname;
            while (fgets(line, sizeof(line), mounts)) {
                // Parse: device mountpoint fs_type ...
                char* dev_tok = strtok(line, " \t\n");
                char* mp_tok = strtok(nullptr, " \t\n");
                if (dev_tok && mp_tok) {
                    std::string dev_str = dev_tok;
                    std::string mp_str = mp_tok;
                    if (dev_str == devpath || (dev_str.size() > dname.size() &&
                        dev_str.compare(dev_str.size() - dname.size(), dname.size(), dname) == 0)) {
                        if (!dev.mountpoint.empty()) dev.mountpoint += " ";
                        dev.mountpoint += mp_str;
                    }
                }
            }
            fclose(mounts);
        }

        // Read children (partitions)
        DIR* sub_dir = opendir((block_dir + "/" + dname).c_str());
        if (sub_dir) {
            struct dirent* child;
            while ((child = readdir(sub_dir)) != nullptr) {
                if (child->d_name[0] == '.') continue;
                std::string cname = child->d_name;
                if (cname == dname) continue;
                // Partitions have a "partition" attribute
                std::string cpath = block_dir + "/" + dname + "/" + cname + "/partition";
                if (access(cpath.c_str(), F_OK) == 0) {
                    dev.children.push_back(cname);
                }
            }
            closedir(sub_dir);
        }

        devices.push_back(dev);
    }

    return devices;
}

static std::string format_size(long long bytes, bool in_bytes) {
    if (in_bytes) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", bytes);
        return buf;
    }
    const char* suffixes[] = {"B", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei"};
    int idx = 0;
    double size = (double)bytes;
    while (size >= 1024.0 && idx < 6) {
        size /= 1024.0;
        idx++;
    }
    char buf[32];
    if (idx == 0) {
        snprintf(buf, sizeof(buf), "%lldB", bytes);
    } else {
        snprintf(buf, sizeof(buf), "%.1f%s", size, suffixes[idx]);
    }
    return buf;
}

// Find device by name
static const BlockDevice* find_device(const std::vector<BlockDevice>& devices, const std::string& name) {
    for (const auto& d : devices) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

// Check if a device is a child of any other device
static bool is_child(const std::vector<BlockDevice>& devices, const std::string& name) {
    for (const auto& d : devices) {
        for (const auto& c : d.children) {
            if (c == name) return true;
        }
    }
    return false;
}

// Print a single device line with prefix
static void print_device_line(const BlockDevice& dev, const std::string& prefix, bool is_last,
                              bool paths, bool bytes, bool noheadings,
                              const std::vector<std::string>& output_cols) {
    // Tree connector
    if (!noheadings) {
        if (is_last) printf("%s└─ ", prefix.c_str());
        else printf("%s├─ ", prefix.c_str());
    }
    
    // Print columns
    for (size_t c = 0; c < output_cols.size(); c++) {
        const char* col = output_cols[c].c_str();
        std::string val;
        if (strcmp(col, "NAME") == 0) {
            val = paths ? ("/dev/" + dev.name) : dev.name;
        } else if (strcmp(col, "SIZE") == 0) {
            val = format_size(dev.size_bytes, bytes);
        } else if (strcmp(col, "TYPE") == 0) {
            val = dev.type;
        } else if (strcmp(col, "MOUNTPOINT") == 0) {
            val = dev.mountpoint;
        } else if (strcmp(col, "MAJ:MIN") == 0) {
            val = dev.maj_min;
        } else {
            val = "";
        }
        printf("%s", val.c_str());
        if (c + 1 < output_cols.size()) printf(" ");
    }
    printf("\n");
}

// Recursively print device tree
static void print_device_tree(const BlockDevice& dev, const std::vector<BlockDevice>& devices,
                              const std::string& prefix, bool is_last,
                              bool paths, bool bytes, bool noheadings,
                              const std::vector<std::string>& output_cols) {
    print_device_line(dev, prefix, is_last, paths, bytes, noheadings, output_cols);
    
    // Print children
    std::string new_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < dev.children.size(); i++) {
        const BlockDevice* child = find_device(devices, dev.children[i]);
        if (child) {
            bool child_last = (i == dev.children.size() - 1);
            print_device_tree(*child, devices, new_prefix, child_last, paths, bytes, noheadings, output_cols);
        }
    }
}

int lsblk_command(int argc, char** argv) {
    bool all = false;
    bool bytes = false;
    bool noheadings = false;
    bool json_mode = false;
    bool paths = false;
    std::vector<std::string> output_cols;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("lsblk");
            return 0;
        }
        if (strcmp(a, "-a") == 0 || strcmp(a, "--all") == 0) {
            all = true;
            continue;
        }
        if (strcmp(a, "-b") == 0 || strcmp(a, "--bytes") == 0) {
            bytes = true;
            continue;
        }
        if (strcmp(a, "-n") == 0 || strcmp(a, "--noheadings") == 0) {
            noheadings = true;
            continue;
        }
        if (strcmp(a, "-J") == 0 || strcmp(a, "--json") == 0) {
            json_mode = true;
            continue;
        }
        if (strcmp(a, "-p") == 0 || strcmp(a, "--paths") == 0) {
            paths = true;
            continue;
        }
        if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
            i++;
            if (i < argc) {
                std::string single = argv[i];
                size_t pos = 0;
                while (pos <= single.size()) {
                    size_t comma = single.find(',', pos);
                    if (comma == std::string::npos) comma = single.size();
                    std::string col = single.substr(pos, comma - pos);
                    size_t s = col.find_first_not_of(" \t");
                    size_t e = col.find_last_not_of(" \t");
                    if (s != std::string::npos) col = col.substr(s, e - s + 1);
                    if (!col.empty()) output_cols.push_back(col);
                    pos = comma + 1;
                }
            }
            continue;
        }
        if (a[0] == '-') {
            fprintf(stderr, "lsblk: unrecognized option '%s'\n", a);
            fprintf(stderr, "Try 'lsblk --help' for more information.\n");
            return 1;
        }
    }

    // Default columns
    if (output_cols.empty()) {
        output_cols = {"NAME", "SIZE", "TYPE", "MOUNTPOINT"};
    }

    auto devices = collect_devices(all);

    if (json_mode) {
        printf("[\n");
        for (size_t i = 0; i < devices.size(); i++) {
            const auto& dev = devices[i];
            printf("  {\n");
            printf("    \"name\": "); json_escape_string(stdout, dev.name.c_str()); printf(",\n");
            printf("    \"size\": %lld,\n", dev.size_bytes);
            printf("    \"type\": "); json_escape_string(stdout, dev.type.c_str()); printf(",\n");
            printf("    \"mountpoint\": "); json_escape_string(stdout, dev.mountpoint.c_str()); printf(",\n");
            printf("    \"children\": [");
            for (size_t j = 0; j < dev.children.size(); j++) {
                if (j > 0) printf(", ");
                json_escape_string(stdout, dev.children[j].c_str());
            }
            printf("]\n");
            printf("  }%s\n", (i + 1 < devices.size()) ? "," : "");
        }
        printf("]\n");
    } else {
        // Check if default output (tree format)
        bool is_default_output = (output_cols == std::vector<std::string>{"NAME", "SIZE", "TYPE", "MOUNTPOINT"});
        
        if (is_default_output) {
            // Tree format - find root devices
            std::vector<size_t> root_indices;
            for (size_t i = 0; i < devices.size(); i++) {
                if (!is_child(devices, devices[i].name)) {
                    root_indices.push_back(i);
                }
            }
            for (size_t i = 0; i < root_indices.size(); i++) {
                bool is_last = (i == root_indices.size() - 1);
                print_device_tree(devices[root_indices[i]], devices, "", is_last, paths, bytes, noheadings, output_cols);
            }

        } else {
            // Text output
            if (!noheadings) {
                for (size_t c = 0; c < output_cols.size(); c++) {
                    const char* col = output_cols[c].c_str();
                    if (strcmp(col, "NAME") == 0) printf("NAME");
                    else if (strcmp(col, "SIZE") == 0) printf("SIZE");
                    else if (strcmp(col, "TYPE") == 0) printf("TYPE");
                    else if (strcmp(col, "MOUNTPOINT") == 0) printf("MOUNTPOINT");
                    else if (strcmp(col, "MAJ:MIN") == 0) printf("MAJ:MIN");
                    else printf("%s", col);
                    if (c + 1 < output_cols.size()) printf(" ");
                }
                printf("\n");
            }

            for (const auto& dev : devices) {
                for (size_t c = 0; c < output_cols.size(); c++) {
                    const char* col = output_cols[c].c_str();
                    std::string val;
                    if (strcmp(col, "NAME") == 0) {
                        val = paths ? ("/dev/" + dev.name) : dev.name;
                    } else if (strcmp(col, "SIZE") == 0) {
                        val = format_size(dev.size_bytes, bytes);
                    } else if (strcmp(col, "TYPE") == 0) {
                        val = dev.type;
                    } else if (strcmp(col, "MOUNTPOINT") == 0) {
                        val = dev.mountpoint;
                    } else if (strcmp(col, "MAJ:MIN") == 0) {
                        val = dev.maj_min;
                    } else {
                        val = "";
                    }
                    printf("%s", val.c_str());
                    if (c + 1 < output_cols.size()) printf(" ");
                }
                printf("\n");
            }
        }
    }

    return 0;
}

REGISTER_COMMAND("lsblk", lsblk_command, "List information about block devices");
