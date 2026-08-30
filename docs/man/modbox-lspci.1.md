% MODBOX-LSPCI(1) modbox | User Commands
% modbox project
% 2026-08-30

# NAME

modbox-lspci — list PCI devices

# SYNOPSIS

**modbox lspci** [*OPTION*]...

# DESCRIPTION

List PCI devices. Reads device attributes from the Linux `/sys/bus/pci/devices/`
sysfs interface (no special privileges required), resolves vendor and device
IDs to human-readable names from the `pci.ids` database, and translates PCI
class codes to readable class names. Devices are listed in address order.

Vendor and device names are resolved from the system `pci.ids` file. When the
database is not present or a name is unknown, the raw hexadecimal ID is shown
in its place. The `MODBOX_IDS_DIR` environment variable overrides the database
directory (default `/usr/share/hwdata`); `MODBOX_SYSFS` overrides the sysfs
root (default `/sys`).

# OPTIONS

**-n**, **--no-name**
:   Show raw hexadecimal vendor/device IDs instead of resolved names.

**-J**, **--json**
:   Output in JSON format. Each entry is a JSON object with fields `address`,
    `class`, `vendor`, and `device`.

**-e**, **--extended**
:   Print an extended, column-aligned readable format.

**--parse**=*LIST*
:   Use the comma-separated *LIST* of fields as output, one value per field
    per line. Recognized fields: `address`, `class`, `vendor`, `device`,
    `svendor`, `sdevice`.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# EXAMPLES

```bash
# List all PCI devices with resolved names
modbox lspci

# List devices using raw vendor/device IDs
modbox lspci -n

# Emit device information as JSON for scripting
modbox lspci --json
```

# EXIT STATUS

`0`
:   Success (including when no devices are present).

non-zero
:   Invalid option or argument, or the sysfs device directory cannot be read.

# SEE ALSO

**modbox-lsusb**(1), **modbox-lscpu**(1), **modbox**(1)
