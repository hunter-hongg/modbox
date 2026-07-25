# 05 — Add sort cycling and richer file-type icons

**What to build:** Sort mode cycling (`s` to advance through name → size → mtime → type, `S` to reverse direction) that persists across directory changes. At the same time, expand the file-type detection from 4 categories to 8 by mapping `S_ISSOCK`, `S_ISFIFO`, `S_ISBLK`, and `S_ISCHR` to distinct icons in the entry list.

**Blocked by:** 03

**Status:** ready-for-agent

- [ ] TuiEntry gains a FileType enum (regular, directory, symlink, socket, fifo, block-device, char-device, unknown)
- [ ] ls_entry_to_tui sets FileType via S_ISREG/S_ISDIR/S_ISLNK/S_ISSOCK/S_ISFIFO/S_ISBLK/S_ISCHR
- [ ] SortMode enum has Name, Size, Mtime, Type; sort_reverse_ bool tracks direction
- [ ] `s` cycles forward through sort modes; `S` toggles reverse
- [ ] Sort applied after every fill_entries(), including after cd into a subdirectory
- [ ] Sort indicator shown in footer (e.g. "Sort: size↓")
- [ ] Each file type renders with a distinct icon in the entry list
