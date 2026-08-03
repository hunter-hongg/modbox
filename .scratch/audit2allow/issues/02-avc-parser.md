# 02 — audit2allow: AVC record parser

**What to build:** A robust parser that reads audit log lines from stdin (or a file via `-i`) and extracts structured data from AVC denial messages. The parser handles both the full `type=AVC msg=audit(...)` format and the short `avc: denied { ... }` format. Non-AVC lines are silently skipped. This ticket delivers correct parsing verified by feeding known input and checking extracted fields.

**Blocked by:** 01 (CLI scaffold must exist so parsed data can be tested end-to-end)

**Status:** ready-for-agent

- [ ] Parser extracts `scontext`, `tcontext`, `tclass`, `perms`, and `comm` from a full audit log line:
  ```
  type=AVC msg=audit(1680000000.000:123): avc:  denied  { read } for  pid=1 comm="systemd" name="passwd" dev="rootfs" ino=12345 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:passwd_file_t:s0 tclass=file permissive=0
  ```
- [ ] Parser extracts the same fields from a short-form AVC line:
  ```
  avc:  denied  { read write } for  pid=1 comm="httpd" srcname="index.html" tclass=file salabel=system_u:system_r:httpd_t:s0 tlabel=system_u:object_r:web_content_t:s0
  ```
- [ ] `perms` are parsed as a sorted, deduplicated list (`{ read write }` → `["read", "write"]`)
- [ ] `tclass` is extracted correctly (`file`, `dir`, `tcp_socket`, etc.)
- [ ] `comm` is extracted from `comm="..."` (supports quoted strings)
- [ ] Non-AVC lines (e.g. `type=SYSCALL ...`, plain text) are silently skipped
- [ ] Lines without `denied` keyword are silently skipped
- [ ] Empty input produces zero parsed records (no crash)
- [ ] Malformed AVC lines (missing fields) are skipped without error
- [ ] The parsed records are stored internally and accessible for the next ticket's rule generation
- [ ] All `test_audit2allow.sh` parsing tests pass
