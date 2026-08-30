#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>

#include "commands/lsusb.hpp"
#include "commands/command_macros.hpp"
#include "commands/ids_database.hpp"
#include "commands/json_stringifier.hpp"
#include "commands/version_util.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Display USB device information. This is the modbox implementation of the standard lsusb utility.\n");
    printf("\n");
    printf("  -n, --no-name           Show raw IDs instead of names\n");
    printf("  -J, --json              Use JSON output format\n");
    printf("      --parse=<list>      Use <list> of fields as output keys and values\n");
    printf("  -h, --help              Display this help and exit\n");
    printf("      --version            Output version information and exit\n");
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

// Parse one USB device directory into a UsbDevice struct.
static bool parse_usb_device(const std::string& sysfs_root,
                             const std::string& ids_dir,
                             const std::string& address,
                             bool use_names,
                             UsbDevice& dev) {
    dev.address = address;
    const std::string base = sysfs_root + "/bus/usb/devices/" + address;

    read_attr(base + "/idVendor", dev.vendor_id);
    read_attr(base + "/idProduct", dev.product_id);
    read_attr(base + "/bDeviceClass", dev.class_code);
    read_attr(base + "/speed", dev.speed);
    std::string busnum_str, devnum_str;
    read_attr(base + "/busnum", busnum_str);
    read_attr(base + "/devnum", devnum_str);
    dev.busnum = busnum_str.empty() ? 0 : std::atoi(busnum_str.c_str());
    dev.devnum = devnum_str.empty() ? 0 : std::atoi(devnum_str.c_str());

    uint8_t bclass = static_cast<uint8_t>(hex_to_uint16(dev.class_code) & 0xFF);
    dev.class_name = usb_class_name(bclass);

    uint16_t vid = hex_to_uint16(dev.vendor_id);
    uint16_t pid = hex_to_uint16(dev.product_id);

    dev.vendor_name = "0x" + dev.vendor_id;
    dev.product_name = "0x" + dev.product_id;

    if (use_names) {
        static IdsDatabase db;
        static bool db_loaded = false;
        if (!db_loaded) {
            db.load((ids_dir + "/usb.ids").c_str());
            db_loaded = true;
        }
        const std::string* vname = db.vendor_name(vid);
        const std::string* pname = db.device_name(vid, pid);
        if (vname) dev.vendor_name = *vname;
        if (pname) dev.product_name = *pname;
    }

    return !dev.vendor_id.empty();
}

// Scan /sys/bus/usb/devices/ for device directories (no ':' in the name).
static void scan_usb_devices(const std::string& sysfs_root, std::vector<std::string>& out) {
    std::string path = sysfs_root + "/bus/usb/devices/";
    DIR* dir = opendir(path.c_str());
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        if (std::string(entry->d_name).find(':') != std::string::npos) continue;
        out.emplace_back(entry->d_name);
    }
    closedir(dir);
}

// Print one device in a given output mode.
static void print_device(const UsbDevice& dev, bool use_names, bool json_mode,
                         const std::vector<std::string>& parse_fields,
                         bool needs_comma) {
    if (!parse_fields.empty()) {
        for (size_t j = 0; j < parse_fields.size(); ++j) {
            const std::string& fld = parse_fields[j];
            if (fld == "address") printf("%s", dev.address.c_str());
            else if (fld == "vendor") printf("%s", use_names ? dev.vendor_name.c_str() : dev.vendor_id.c_str());
            else if (fld == "product") printf("%s", use_names ? dev.product_name.c_str() : dev.product_id.c_str());
            else if (fld == "class") printf("%s", dev.class_name.c_str());
            else if (fld == "speed") printf("%s", dev.speed.c_str());
            else printf("<unknown>");
            if (j + 1 < parse_fields.size()) printf(" ");
        }
        printf("\n");
        return;
    }

    if (json_mode) {
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
        printf(", \"product\": ");
        if (use_names) {
            json_escape_string(stdout, dev.product_name.c_str());
        } else {
            printf("\"0x%s\"", dev.product_id.c_str());   // hex-only, safe
        }
        printf("}");
        if (needs_comma) printf(",");
        printf("\n");
        return;
    }

    // Default text format, matching real lsusb: "Bus 001 Device 002: ID vid:pid Vendor".
    // Fall back to the sysfs address when busnum/devnum are unavailable.
    if (dev.busnum > 0 || dev.devnum > 0) {
        printf("Bus %03d Device %03d: ID %s:%s", dev.busnum, dev.devnum,
               dev.vendor_id.c_str(), dev.product_id.c_str());
    } else {
        printf("%s: ID %s:%s", dev.address.c_str(), dev.vendor_id.c_str(), dev.product_id.c_str());
    }
    if (use_names) {
        printf(" %s", dev.vendor_name.c_str());
    }
    printf("\n");
}

int lsusb_command(int argc, char** argv) {
    bool no_name = false;
    bool json_mode = false;
    std::vector<std::string> parse_fields;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }
        if (strcmp(a, "--version") == 0) {
            print_version("lsusb");
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
    scan_usb_devices(sysfs_root, addresses);
    std::sort(addresses.begin(), addresses.end());

    // Collect successfully-parsed devices first so JSON comma bookkeeping
    // is based on the actual emitted count, not the raw directory count.
    std::vector<UsbDevice> devices;
    for (const auto& addr : addresses) {
        UsbDevice dev;
        if (parse_usb_device(sysfs_root, ids_dir, addr, use_names, dev)) {
            devices.push_back(std::move(dev));
        }
    }

    bool json_list = (parse_fields.empty() && json_mode);
    if (json_list) {
        printf("{\n  \"devices\": [\n");
    }
    for (size_t i = 0; i < devices.size(); ++i) {
        bool needs_comma = (i + 1 < devices.size());
        print_device(devices[i], use_names, json_mode, parse_fields, needs_comma);
    }
    if (json_list) {
        printf("  ]\n}\n");
    }

    return 0;
}

REGISTER_COMMAND("lsusb", lsusb_command, "Display USB device information");
