#ifndef LSUSB_HPP
#define LSUSB_HPP

#include <cstdint>
#include <string>

// USB device record from /sys/bus/usb/devices/<bus-port>
struct UsbDevice {
    std::string address;            // e.g., "1-1" or "1-1.2"
    int busnum = 0;                 // bus number (decimal)
    int devnum = 0;                 // device number (decimal)
    std::string vendor_id;          // hex string (e.g., "17ef")
    std::string product_id;         // hex string
    std::string class_code;         // bDeviceClass hex string
    std::string speed;              // USB speed (Mbps)

    // Resolved names
    std::string vendor_name;        // resolved via usb.ids
    std::string product_name;
    std::string class_name;         // from embedded class table
};

// USB class name lookup (bDeviceClass).
static inline const char* usb_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Use class info in interface descriptor";
        case 0x01: return "Audio";
        case 0x02: return "Communications and CDC Control";
        case 0x03: return "HID";
        case 0x05: return "Physical";
        case 0x06: return "Image";
        case 0x07: return "Printer";
        case 0x08: return "Mass Storage";
        case 0x09: return "Hub";
        case 0x0A: return "CDC Data";
        case 0x0B: return "Smart Card";
        case 0x0D: return "Content Security";
        case 0x0E: return "Video";
        case 0x0F: return "Personal Healthcare";
        case 0x10: return "Audio/Video";
        case 0x11: return "Billboard";
        case 0x12: return "USB Type-C Bridge";
        case 0xDC: return "Diagnostic";
        case 0xE0: return "Wireless Controller";
        case 0xEF: return "Miscellaneous";
        case 0xFE: return "Application Specific";
        case 0xFF: return "Vendor Specific";
        default: return "Unknown";
    }
}

#endif /* LSUSB_HPP */
