% MODBOX-LSUSB(1) modbox | User Commands
% modbox project
% 2026-08-30

# NAME

modbox-lsusb — list USB devices

# SYNOPSIS

**modbox lsusb** [*OPTION*]...

# DESCRIPTION

List USB devices. Reads device attributes from the Linux
`/sys/bus/usb/devices/` sysfs interface (no special privileges required),
resolves vendor and product IDs to human-readable names from the `usb.ids`
database, and translates USB class codes to readable class names. Only device
nodes are listed (interface entries are skipped).

Vendor names are resolved from the system `usb.ids` file. When the database is
not present or a name is unknown, the raw hexadecimal ID is shown in its place.
The `MODBOX_IDS_DIR` environment variable overrides the database directory
(default `/usr/share/hwdata`); `MODBOX_SYSFS` overrides the sysfs root
(default `/sys`).

# OPTIONS

**-n**, **--no-name**
:   Show raw hexadecimal vendor/product IDs instead of resolved names.

**-J**, **--json**
:   Output in JSON format. Each entry is a JSON object with fields `address`,
    `class`, `vendor`, and `product`.

**--parse**=*LIST*
:   Use the comma-separated *LIST* of fields as output, one value per field
    per line. Recognized fields: `address`, `vendor`, `product`, `class`,
    `speed`.

**-h**, **--help**
:   Display help and exit.

**--version**
:   Display version and exit.

# EXAMPLES

```bash
# List all USB devices with resolved vendor names
modbox lsusb

# List devices using raw vendor/product IDs
modbox lsusb -n

# Emit device information as JSON for scripting
modbox lsusb --json
```

# EXIT STATUS

`0`
:   Success (including when no devices are present).

non-zero
:   Invalid option or argument, or the sysfs device directory cannot be read.

# SEE ALSO

**modbox-lspci**(1), **modbox**(1)
