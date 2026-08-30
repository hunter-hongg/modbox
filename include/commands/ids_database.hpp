#ifndef IDS_DATABASE_HPP
#define IDS_DATABASE_HPP

// Shared parser for the `pci.ids`/`usb.ids` hardware ID databases.
//
// Both databases share an identical record layout, so a single loader resolves
// either. The nesting depth (number of leading tabs) is significant:
//
//   vendor line:   "8086  Intel Corporation"     (column 0, 4 hex + space + name)
//   device line:   "\t5917  Kaby Lake-R GT2 …"   (one tab = depth 1)
//   subsystem line:"\t\t5917  …"                (two tabs = depth 2, skipped)
//   section header:"C 00 …" / "AT 01 …"         (column 0, NOT 4 hex, skipped)
//
// Only depth-1 lines are registered as devices/products; deeper lines
// (subsystem, interface, prog-if) and column-0 section headers are ignored,
// so a device's name is never clobbered by a nested subsystem entry.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

// Decode the first four hex digits of s into out (max 4 digits), stopping at
// the first non-hex character. Returns 0 on empty/garbage. Shared by lspci
// and lsusb for vendor/device/class id decoding.
static inline uint16_t hex_to_uint16(const std::string& s) {
    uint16_t val = 0;
    for (size_t i = 0; i < 4 && i < s.size(); ++i) {
        char c = s[i];
        uint8_t nibble = 0xFF;
        if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') nibble = static_cast<uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8_t>(c - 'A' + 10);
        if (nibble == 0xFF) return 0;
        val = static_cast<uint16_t>((val << 4) | nibble);
    }
    return val;
}

class IdsDatabase {
public:
    // Parse one ids file. Returns true if the file was opened and at least
    // partially parsed; false if the file is missing or unreadable (the
    // caller falls back to raw hex IDs).
    bool load(const char* filepath) {
        FILE* f = std::fopen(filepath, "r");
        if (!f) return false;
        vendors_.clear();
        devices_.clear();
        char line[1024];
        uint16_t cur_vendor = 0;
        bool have_vendor = false;
        while (std::fgets(line, sizeof(line), f)) {
            // Strip trailing newline.
            size_t len = std::strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[--len] = '\0';
            }
            if (len == 0) continue;
            if (line[0] == '#') continue;

            // Count leading tabs = nesting depth.
            size_t depth = 0;
            while (depth < len && line[depth] == '\t') {
                ++depth;
            }

            if (depth == 1) {
                // Device/product line: one tab, then a 4-hex id, then the name.
                if (!have_vendor) continue;
                const char* p = line + 1;
                uint16_t did = 0;
                if (!parse_hex4(p, did)) continue;
                if (*p != ' ' && *p != '\t') continue;   // must be id + separator
                while (*p == ' ' || *p == '\t') ++p;
                if (*p != '\0') add_device(cur_vendor, did, p);
            } else if (depth == 0) {
                // Column-0 line. A vendor header is exactly 4 hex digits
                // followed by whitespace. Section headers ("C ", "AT ",
                // "HID ", "BIAS ") and anything else fail this test and are
                // skipped.
                const char* p = line;
                uint16_t vid = 0;
                if (parse_hex4(p, vid) && (*p == ' ' || *p == '\t')) {
                    while (*p == ' ' || *p == '\t') ++p;
                    if (*p != '\0') {
                        add_vendor(vid, p);
                        cur_vendor = vid;
                        have_vendor = true;
                    }
                }
            }
            // depth >= 2: subsystem / interface / prog-if lines — skip.
        }
        std::fclose(f);
        return true;
    }

    // Resolve a vendor name by id. Returns nullptr if unknown.
    const std::string* vendor_name(uint16_t vid) const {
        auto it = vendors_.find(vid);
        return it == vendors_.end() ? nullptr : &it->second;
    }

    // Resolve a device/product name by (vendor, device) ids.
    const std::string* device_name(uint16_t vid, uint16_t did) const {
        auto it = devices_.find(key(vid, did));
        return it == devices_.end() ? nullptr : &it->second;
    }

private:
    static uint32_t key(uint16_t vid, uint16_t did) {
        return (static_cast<uint32_t>(vid) << 16) | static_cast<uint32_t>(did);
    }

    // Parse exactly four hex digits at the start of s, advancing s past them.
    // Returns false if fewer than four consecutive hex digits are present.
    static bool parse_hex4(const char*& s, uint16_t& out) {
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s[i];
            uint8_t nibble = 0;
            if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
            else if (c >= 'a' && c <= 'f') nibble = static_cast<uint8_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8_t>(c - 'A' + 10);
            else return false;
            val = (val << 4) | nibble;
        }
        s += 4;
        out = static_cast<uint16_t>(val);
        return true;
    }

    void add_vendor(uint16_t vid, const char* name) {
        vendors_[vid] = trim(name);
    }
    void add_device(uint16_t vid, uint16_t did, const char* name) {
        devices_[key(vid, did)] = trim(name);
    }

    static std::string trim(const char* s) {
        const char* start = s;
        while (*start == ' ' || *start == '\t') ++start;
        const char* end = start + std::strlen(start);
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) --end;
        return std::string(start, static_cast<size_t>(end - start));
    }

    std::unordered_map<uint16_t, std::string> vendors_;
    std::unordered_map<uint32_t, std::string> devices_;
};

// Resolve the ids database directory:
//   1. MODBOX_IDS_DIR env var (test/override)
//   2. /usr/share/hwdata
// A missing db is non-fatal (names fall back to raw IDs).
inline std::string get_ids_dir() {
    if (const char* env = std::getenv("MODBOX_IDS_DIR")) {
        if (env[0] != '\0') return std::string(env);
    }
    return std::string("/usr/share/hwdata");
}

// Resolve the sysfs root (default "/sys"); MODBOX_SYSFS overrides for tests.
inline std::string get_sysfs_root() {
    if (const char* env = std::getenv("MODBOX_SYSFS")) {
        if (env[0] != '\0') return std::string(env);
    }
    return std::string("/sys");
}

#endif /* IDS_DATABASE_HPP */
