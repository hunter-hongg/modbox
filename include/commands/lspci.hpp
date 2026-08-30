#ifndef LSPCI_HPP
#define LSPCI_HPP

#include <cstdint>
#include <string>

// PCI device record from /sys/bus/pci/devices/<bdf>
struct PciDevice {
    std::string address;               // e.g., "00:02.0" (single-domain short form)
    std::string class_code;            // hex string from sysfs, 0x stripped (e.g., "030000")
    std::string vendor_id;             // hex string, 0x stripped
    std::string device_id;             // hex string, 0x stripped
    std::string subsystem_vendor_id;   // hex string, 0x stripped
    std::string subsystem_device_id;   // hex string, 0x stripped
    std::string revision;              // hex string, 0x stripped (e.g., "08")

    // Resolved names (raw hex "0xNNNN" if unknown)
    std::string vendor_name;
    std::string device_name;
    std::string class_name;            // from embedded class table
};

// Match a PCI class:subclass code (top 4 hex digits of the class code) to a
// human-readable name. Sourced from the pci.ids "C" section (base + subclass
// entries). The base-class fallback covers subclasses not individually named.
static inline const char* pci_class_name(uint16_t cs) {
    switch (cs) {
        case 0x0000: return "Non-VGA unclassified device";
        case 0x0001: return "VGA compatible unclassified device";
        case 0x0005: return "Image coprocessor";
        case 0x0100: return "SCSI storage controller";
        case 0x0101: return "IDE interface";
        case 0x0102: return "Floppy disk controller";
        case 0x0103: return "IPI bus controller";
        case 0x0104: return "RAID bus controller";
        case 0x0105: return "ATA controller";
        case 0x0106: return "SATA controller";
        case 0x0107: return "Serial Attached SCSI controller";
        case 0x0108: return "Non-Volatile memory controller";
        case 0x0109: return "Universal Flash Storage controller";
        case 0x0180: return "Mass storage controller";
        case 0x0200: return "Ethernet controller";
        case 0x0201: return "Token ring network controller";
        case 0x0202: return "FDDI network controller";
        case 0x0203: return "ATM network controller";
        case 0x0204: return "ISDN controller";
        case 0x0205: return "WorldFip controller";
        case 0x0206: return "PICMG controller";
        case 0x0207: return "Infiniband controller";
        case 0x0208: return "Fabric controller";
        case 0x0280: return "Network controller";
        case 0x0300: return "VGA compatible controller";
        case 0x0301: return "XGA compatible controller";
        case 0x0302: return "3D controller";
        case 0x0380: return "Display controller";
        case 0x0400: return "Multimedia video controller";
        case 0x0401: return "Multimedia audio controller";
        case 0x0402: return "Computer telephony device";
        case 0x0403: return "Audio device";
        case 0x0480: return "Multimedia controller";
        case 0x0500: return "RAM memory";
        case 0x0501: return "FLASH memory";
        case 0x0502: return "CXL";
        case 0x0580: return "Memory controller";
        case 0x0600: return "Host bridge";
        case 0x0601: return "ISA bridge";
        case 0x0602: return "EISA bridge";
        case 0x0603: return "MicroChannel bridge";
        case 0x0604: return "PCI bridge";
        case 0x0605: return "PCMCIA bridge";
        case 0x0606: return "NuBus bridge";
        case 0x0607: return "CardBus bridge";
        case 0x0608: return "RACEway bridge";
        case 0x0609: return "Semi-transparent PCI-to-PCI bridge";
        case 0x060A: return "InfiniBand to PCI host bridge";
        case 0x0680: return "Bridge";
        case 0x0700: return "Serial controller";
        case 0x0701: return "Parallel controller";
        case 0x0702: return "Multiport serial controller";
        case 0x0703: return "Modem";
        case 0x0704: return "GPIB controller";
        case 0x0705: return "Smard Card controller";
        case 0x0780: return "Communication controller";
        case 0x0800: return "PIC";
        case 0x0801: return "DMA controller";
        case 0x0802: return "Timer";
        case 0x0803: return "RTC";
        case 0x0804: return "PCI Hot-plug controller";
        case 0x0805: return "SD Host controller";
        case 0x0806: return "IOMMU";
        case 0x0807: return "Root Complex Event Collector";
        case 0x0880: return "System peripheral";
        case 0x0900: return "Keyboard controller";
        case 0x0901: return "Digitizer Pen";
        case 0x0902: return "Mouse controller";
        case 0x0903: return "Scanner controller";
        case 0x0904: return "Gameport controller";
        case 0x0980: return "Input device controller";
        case 0x0A00: return "Generic Docking Station";
        case 0x0A80: return "Docking Station";
        case 0x0B00: return "386";
        case 0x0B01: return "486";
        case 0x0B02: return "Pentium";
        case 0x0B10: return "Alpha";
        case 0x0B20: return "Power PC";
        case 0x0B30: return "MIPS";
        case 0x0B40: return "Co-processor";
        case 0x0C00: return "FireWire (IEEE 1394)";
        case 0x0C01: return "ACCESS Bus";
        case 0x0C02: return "SSA";
        case 0x0C03: return "USB controller";
        case 0x0C04: return "Fibre Channel";
        case 0x0C05: return "SMBus";
        case 0x0C06: return "InfiniBand";
        case 0x0C07: return "IPMI Interface";
        case 0x0C08: return "SERCOS interface";
        case 0x0C09: return "CANBUS";
        case 0x0C0A: return "MIPI I3C";
        case 0x0C80: return "Serial bus controller";
        case 0x0D00: return "IRDA controller";
        case 0x0D01: return "Consumer IR controller";
        case 0x0D10: return "RF controller";
        case 0x0D11: return "Bluetooth";
        case 0x0D12: return "Broadband";
        case 0x0D20: return "802.11a 5 GHz controller";
        case 0x0D21: return "802.11b 2.4 GHz controller";
        case 0x0D40: return "Cellular controller/modem";
        case 0x0D80: return "Wireless controller";
        case 0x0E00: return "I2O";
        case 0x0F01: return "Satellite TV controller";
        case 0x0F02: return "Satellite audio communication controller";
        case 0x0F03: return "Satellite voice communication controller";
        case 0x0F04: return "Satellite data communication controller";
        case 0x1000: return "Network and computing encryption device";
        case 0x1010: return "Entertainment encryption device";
        case 0x1080: return "Encryption controller";
        case 0x1100: return "DPIO module";
        case 0x1101: return "Performance counters";
        case 0x1110: return "Communication synchronizer";
        case 0x1120: return "Signal processing management";
        case 0x1180: return "Signal processing controller";
        case 0x1200: return "Processing accelerators";
        case 0x1201: return "SNIA Smart Data Accelerator Interface (SDXI) controller";
        default: {
            // Fall back to the base class (top byte) for unnamed subclasses.
            uint8_t base = static_cast<uint8_t>(cs >> 8);
            switch (base) {
                case 0x00: return "Unclassified device";
                case 0x01: return "Mass storage controller";
                case 0x02: return "Network controller";
                case 0x03: return "Display controller";
                case 0x04: return "Multimedia controller";
                case 0x05: return "Memory controller";
                case 0x06: return "Bridge";
                case 0x07: return "Communication controller";
                case 0x08: return "Generic system peripheral";
                case 0x09: return "Input device controller";
                case 0x0A: return "Docking station";
                case 0x0B: return "Processor";
                case 0x0C: return "Serial bus controller";
                case 0x0D: return "Wireless controller";
                case 0x0E: return "Intelligent controller";
                case 0x0F: return "Satellite communications controller";
                case 0x10: return "Encryption controller";
                case 0x11: return "Signal processing controller";
                case 0x12: return "Processing accelerators";
                case 0x13: return "Non-Essential Instrumentation";
                case 0x40: return "Coprocessor";
                case 0xFF: return "Unassigned class";
                default: return "Unknown";
            }
        }
    }
}

#endif /* LSPCI_HPP */
