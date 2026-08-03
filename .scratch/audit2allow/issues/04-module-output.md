# 04 — audit2allow: module output and require blocks

**What to build:** Module-format output (`-m MODULE_NAME`) that wraps allow rules in a SELinux module declaration with a `require` block. The `require` block lists types and classes/permissions that may not be in the current policy. Also support `-r`/`--requires` for require-only output and `-m` without `-r` (reference implies `-r` when `-m` is used).

**Blocked by:** 03 (needs rule generation to work before wrapping in module format)

**Status:** ready-for-agent

- [ ] `-m testmod` wraps output in module format:
  ```
  module testmod 1.0;

  require {
      type init_t;
      type passwd_file_t;
      class file read;
  }

  #============= init_t ==============
  allow init_t passwd_file_t:file read;
  ```
- [ ] `-m testmod -r` produces the same module output (reference behavior: `-m` implies `-r`)
- [ ] `-r` without `-m` emits only `require` blocks followed by allow rules (no module declaration):
  ```
  require {
      type init_t;
      class file read;
  }

  allow init_t passwd_file_t:file read;
  ```
- [ ] `require` block types are sorted alphabetically
- [ ] `require` block classes+perms are sorted alphabetically
- [ ] Multiple perms for the same class are grouped: `class file { read write };`
- [ ] `-M test` (module package) prints error and exits non-zero (out of scope)
- [ ] `-M` conflicts with `-o` and `-m` → error message
- [ ] `-m` and `-o` together: module output is written to the output file
- [ ] All `test_audit2allow.sh` module output tests pass
