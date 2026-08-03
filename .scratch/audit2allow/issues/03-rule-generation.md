# 03 — audit2allow: traditional allow rule output

**What to build:** Rule generation that takes parsed AVC denials and emits traditional SELinux policy `allow` rules. Deduplication is applied: identical `(source_type, target_type, tclass, perms)` combinations are grouped into a single rule. Output is grouped by `comm` (process name) with comment headers, matching the reference implementation's format.

**Blocked by:** 02 (needs parsed AVC records to generate rules from)

**Status:** ready-for-agent

- [ ] Single AVC denial produces a single allow rule:
  ```
  #============= init_t ==============
  allow init_t passwd_file_t:file read;
  ```
- [ ] Multiple different denials for the same types/class produce grouped perms:
  ```
  #============= init_t ==============
  allow init_t passwd_file_t:file { read write };
  ```
- [ ] Deduplication: two identical denials produce exactly one rule (not two)
- [ ] Different source types produce separate rule blocks with separate comment headers
- [ ] Different target classes produce separate rules even with same types
- [ ] Output format matches reference: `allow src_type tgt_type:tclass { perms };`
- [ ] Comment header format matches reference: `#============= comm ==============`
- [ ] When `-o FILE` is used, rules are appended to the file (not overwriting)
- [ ] Default output goes to stdout when no `-o` is specified
- [ ] No output is produced for non-AVC input lines (verified end-to-end)
- [ ] All `test_audit2allow.sh` output format tests pass
