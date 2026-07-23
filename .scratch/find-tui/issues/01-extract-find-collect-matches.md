# 01 — Extract find_collect_matches collector

**What to build:** A new `find_collect_matches` function that drives the same walk-and-evaluate pipeline already used by `find -print`, but instead of printing each match it populates a `std::vector<FindMatch>` and returns it. The existing `find_walk`/`find_evaluate`/`find_exec_file` code path is untouched — this adds a new parallel path alongside it.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] `FindMatch` struct defined with `path`, `display_name`, `file_type`, `size`, `mtime`, `mtime_str`, `perm_str`
- [ ] `find_collect_matches(starting_points, opts)` function implemented using the existing predicate and walk logic, returning `std::vector<FindMatch>`
- [ ] Result set contains correct metadata for matching files (type classification, size, mtime formatted)
- [ ] All existing `test_find.sh` cases still pass (no regression on `-print` / `-delete` / `-exec` paths)
