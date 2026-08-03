# 05 — audit2allow: dontaudit, type filter, and why mode

**What to build:** Three additional output modes that extend the rule generator: `-D`/`--dontaudit` to emit `dontaudit` rules instead of `allow`, `-t REGEX`/`--type=REGEX` to filter which audit message types are processed, and `-w`/`--why` to produce human-readable explanations instead of policy rules.

**Blocked by:** 03 (rule generation foundation must be in place)

**Status:** ready-for-agent

- [ ] `-D` generates `dontaudit` rules instead of `allow`:
  ```
  dontaudit init_t passwd_file_t:file read;
  ```
- [ ] `-D` with `-m testmod` wraps dontaudit rules in module format
- [ ] `-D` with `-r` includes require block before dontaudit rules
- [ ] `-t AVC` filters to only process lines matching the regex (default behavior matches all AVC lines)
- [ ] `-t SYSCALL` with AVC-only input produces no output (filter excludes all lines)
- [ ] `-w`/`--why` produces human-readable explanation instead of policy:
  ```
  # avc:  denied  { read } for pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
      # comm=systemd  name=passwd  dev=rootfs  ino=12345
      # source system_u:system_r:init_t:s0
      # target system_u:object_r:passwd_file_t:s0
      # known false positives: 0
  ```
- [ ] `-w` and `-m` together: why output is produced (not module rules)
- [ ] `-e`/`--explain` is accepted but produces the same output as `-w` in v1 (or warns and falls back)
- [ ] `-v`/`--verbose` is accepted with a warning in v1 (no-op)
- [ ] All `test_audit2allow.sh` extended mode tests pass
- [ ] Build compiles and links without new dependencies (libaudit dev headers not needed)
