#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>
#include <vector>

#include "commands/lspci.hpp"
#include "commands/command_macros.hpp"
#include "commands/ids_database.hpp"
#include "commands/json_stringifier.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Display PCI device information. This is the modbox implementation of the standard lspci utility.\n");
    printf("\n");
    printf("  -n, --no-name           Show raw IDs instead of names\n");
    printf("  -J, --json              Use JSON output format\n");
    printf("  -e, --extended[=COLS]   Print extended readable format\n");
    printf("      --parse=<list>      Use <list> of fields as output keys and values\n");
    printf("  -h, --help              Display this help and exit\n");
    printf("      --version           Output version information and exit\n");
}

// Read a single sysfs attribute file, returning the trimmed content.
static bool read_attr(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::getline(f, out);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
        out.pop_back();
    }
    return true;
}

// Strip a leading "0x"/"0X" prefix from a sysfs hex attribute value.
static std::string strip_hex_prefix(const std::string& s) {
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return s.substr(2);
    }
    return s;
}

// Shorten a full BDF address (e.g. "0000:00:1f.2") to the single-domain short
// form (e.g. "00:1f.2"), matching the default real lspci display.
static std::string shorten_pci_address(const std::string& addr) {
    static const std::string dom = "0000:";
    if (addr.compare(0, dom.size(), dom) == 0) {
        return addr.substr(dom.size());
    }
    return addr;
}

// Parse a sysfs PCI device directory into a PciDevice struct.
// Returns false if the directory is not a valid PCI device (no vendor id).
static bool parse_pci_device(const std::string& sysfs_root,
                             const std::string& ids_dir,
                             const std::string& address,
                             bool use_names,
                             PciDevice& dev) {
    dev.address = shorten_pci_address(address);
    const std::string base = sysfs_root + "/bus/pci/devices/" + address;
    read_attr(base + "/vendor", dev.vendor_id);
    read_attr(base + "/device", dev.device_id);
    read_attr(base + "/class", dev.class_code);
    read_attr(base + "/subsystem_vendor", dev.subsystem_vendor_id);
    read_attr(base + "/subsystem_device", dev.subsystem_device_id);
    read_attr(base + "/revision", dev.revision);

    // Strip the "0x" prefix sysfs prepends to hex values.
    dev.vendor_id = strip_hex_prefix(dev.vendor_id);
    dev.device_id = strip_hex_prefix(dev.device_id);
    dev.class_code = strip_hex_prefix(dev.class_code);
    dev.subsystem_vendor_id = strip_hex_prefix(dev.subsystem_vendor_id);
    dev.subsystem_device_id = strip_hex_prefix(dev.subsystem_device_id);
    dev.revision = strip_hex_prefix(dev.revision);

    // Resolve class name from the class:subclass code.
    dev.class_name = pci_class_name(hex_to_uint16(dev.class_code));

    // Default: raw hex with 0x prefix.
    dev.vendor_name = "0x" + dev.vendor_id;
    dev.device_name = "0x" + dev.device_id;

    if (use_names) {
        static IdsDatabase db;
        static bool db_loaded = false;
        if (!db_loaded) {
            db.load((ids_dir + "/pci.ids").c_str());
            db_loaded = true;
        }
        uint16_t vid = hex_to_uint16(dev.vendor_id);
        uint16_t did = hex_to_uint16(dev.device_id);
        const std::string* vname = db.vendor_name(vid);
        const std::string* dname = db.device_name(vid, did);
        if (vname) dev.vendor_name = *vname;
        if (dname) dev.device_name = *dname;
    }

    return !dev.vendor_id.empty();
}

// Scan /sys/bus/pci/devices/ for PCI device addresses.
static void scan_pci_devices(const std::string& sysfs_root, std::vector<std::string>& out) {
    std::string path = sysfs_root + "/bus/pci/devices/";
    DIR* dir = opendir(path.c_str());
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        out.emplace_back(entry->d_name);
    }
    closedir(dir);
}

int lspci_command(int argc, char** argv) {
    bool no_name = false;
    bool json_mode = false;
    bool extended = false;
    std::vector<std::string> parse_fields;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("lspci");
            return 0;
        }
        if (strcmp(a, "-n") == 0 || strcmp(a, "--no-name") == 0) {
            no_name = true;
            continue;
        }
        if (strcmp(a, "-J") == 0 || strcmp(a, "--json") == 0) {
            json_mode = true;
            continue;
        }
        if (strcmp(a, "-e") == 0 || strcmp(a, "--extended") == 0) {
            extended = true;
            continue;
        }
        if (strncmp(a, "--extended=", 11) == 0) {
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
                size_t s = field.find_first_not_of(" \t");
                size_t e = field.find_last_not_of(" \t");
                if (s != std::string::npos) field = field.substr(s, e - s + 1);
                if (!field.empty()) parse_fields.push_back(field);
                pos = comma + 1;
            }
            continue;
        }
        if (a[0] == '-') {
            fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], a);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    std::string sysfs_root = get_sysfs_root();
    std::string ids_dir = get_ids_dir();
    bool use_names = !no_name;

    std::vector<std::string> addresses;
    scan_pci_devices(sysfs_root, addresses);
    std::sort(addresses.begin(), addresses.end());

    // Collect successfully-parsed devices first so JSON comma bookkeeping is
    // based on the actual emitted count, not the raw directory count.
    std::vector<PciDevice> devices;
    for (const auto& addr : addresses) {
        PciDevice dev;
        if (parse_pci_device(sysfs_root, ids_dir, addr, use_names, dev)) {
            devices.push_back(std::move(dev));
        }
    }

    bool json_list = (parse_fields.empty() && json_mode);
    if (json_list) {
        printf("{\n  \"devices\": [\n");
    }
    for (size_t i = 0; i < devices.size(); ++i) {
        const PciDevice& dev = devices[i];
        bool needs_comma = (i + 1 < devices.size());

        if (json_list) {
            printf("    {\"address\": ");
            json_escape_string(stdout, dev.address.c_str());
            printf(", \"class\": ");
            json_escape_string(stdout, dev.class_name.c_str());
            printf(", \"vendor\": ");
            if (use_names) {
                json_escape_string(stdout, dev.vendor_name.c_str());
            } else {
                printf("\"0x%s\"", dev.vendor_id.c_str());   // hex-only, safe
            }
            printf(", \"device\": ");
            if (use_names) {
                json_escape_string(stdout, dev.device_name.c_str());
            } else {
                printf("\"0x%s\"", dev.device_id.c_str());   // hex-only, safe
            }
            printf("}");
            if (needs_comma) printf(",");
            printf("\n");
        } else if (!parse_fields.empty()) {
            // --parse mode: one line per device, fields space-separated.
            for (size_t j = 0; j < parse_fields.size(); ++j) {
                const std::string& f = parse_fields[j];
                if (f == "address") printf("%s", dev.address.c_str());
                else if (f == "class") printf("%s", dev.class_name.c_str());
                else if (f == "vendor") printf("%s", use_names ? dev.vendor_name.c_str() : dev.vendor_id.c_str());
                else if (f == "device") printf("%s", use_names ? dev.device_name.c_str() : dev.device_id.c_str());
                else if (f == "svendor") printf("%s", dev.subsystem_vendor_id.c_str());
                else if (f == "sdevice") printf("%s", dev.subsystem_device_id.c_str());
                else printf("<unknown>");
                if (j + 1 < parse_fields.size()) printf(" ");
            }
            printf("\n");
        } else if (extended) {
            // Extended format: address, class, vendor, device (column-aligned).
            printf("%-12s %-30s %s %s\n",
                   dev.address.c_str(),
                   dev.class_name.c_str(),
                   (use_names ? dev.vendor_name.c_str() : ("0x" + dev.vendor_id).c_str()),
                   (use_names ? dev.device_name.c_str() : ("0x" + dev.device_id).c_str()));
        } else {
            // Default text format: matches real lspci output.
            printf("%s %s", dev.address.c_str(), dev.class_name.c_str());
            if (use_names) {
                printf(": %s %s", dev.vendor_name.c_str(), dev.device_name.c_str());
            } else {
                printf(": 0x%s 0x%s", dev.vendor_id.c_str(), dev.device_id.c_str());
            }
            if (!dev.revision.empty() && dev.revision != "00") {
                printf(" (rev %s)", dev.revision.c_str());
            }
            printf("\n");
        }
    }

    if (json_list) {
        printf("  ]\n}\n");
    }

    return 0;
}

REGISTER_COMMAND("lspci", lspci_command, "Display PCI device information");
