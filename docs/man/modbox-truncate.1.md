---
title: modbox-truncate
section: 1
date: 2026-08-19
author: modbox project
---

# NAME

modbox-truncate - shrink or extend a file to the specified size

# SYNOPSIS

**modbox truncate** [**OPTION**]... **FILE**...

# DESCRIPTION

Shrink or extend each **FILE** to the specified size.
If the file is shortened, extra data is discarded. If extended, empty space is added.

# OPTIONS

**-c**, **--no-create**
:   Do not create any files.

**-o**, **--io-blocks**
:   Treat SIZE as IO blocks, not bytes. Each block is typically 512 bytes.

**-r**, **--reference=FILE**
:   Base the size on FILE's size instead of specifying SIZE directly.

**-s**, **--size=SIZE**
:   Desired file size. SIZE may be followed/preceded by a modifier:
    - `+` increase by SIZE
    - `-` decrease by SIZE
    - `<` at most SIZE
    - `>` at least SIZE
    - `/` floor to multiple of SIZE
    - `%` ceiling to multiple of SIZE

SIZE may have a suffix:
- `K`, `M`, `G`, `T`, `P`, `E`, `Z`, `Y`, `R`, `Q` (1024^N)
- `KB`, `MB`, ... (1000^N)

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# EXAMPLES

```bash
# Create a 100-byte file
modbox truncate -s 100 file.txt

# Extend file to 1 megabyte
modbox truncate -s 1M data.bin

# Shrink file to 500 bytes
modbox truncate -s 500 large.txt

# Make file same size as reference
modbox truncate -r reference.txt target.txt

# Increase file size by 1GB
modbox truncate -s +1G bigfile.dat
```

# EXIT STATUS

`0` on success, non-zero on error.

# NOTES

- Without **-c**, missing files are created.
- The file is created with size 0 if it doesn't exist.
- Extended files may contain "holes" (unallocated blocks) on some filesystems.

# SEE ALSO

**modbox-dd**(1), **modbox-touch**(1), **modbox-cat**(1)
