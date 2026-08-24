# ADR-014: Man Page Coverage Complete

Date: 2026-08-23

Status: Accepted

## Context
modbox 提供 154 个注册命令，man 页面覆盖率仅 ~65%（100/154）。缺少 54 个常用命令的手册页，影响文档完整性和用户体验。

## Decision
补齐全部 54 个缺失命令的 man 页面：ip, join, link, logname, lsblk, lsc, lscpu, mkfifo, mknod, mtop, nice, nohup, nproc, nslookup, numfmt, od, pathchk, pinky, pr, printenv, printf, prompts, ps, ptx, realpath, rev, rsync, runcon, setfacl, sh, shred, shuf, split, ss, stdbuf, stty, sync, tac, test, time, top, true, tsort, tty, unexpand, unlink, uptime, users, vdir, wall, wget, who, yes, zoxide。

每份文档基于对应 `src/commands/<cmd>.cpp` 的 `print_help` 与 argtable3 定义提取真实选项，保持与现有 `modbox-cat.1.md` 风格一致。

## Changes
- `docs/man/modbox-*.1.md` 新增 54 个模板并扩充为完整文档
- `Makefile` `MAN_SOURCES` 增加 54 条目
- `tests/test_man_pages.sh` 增加 54 条存在性检查
- `make man` 生成 155 个 roff 文件，测试全绿

## Consequences
- Man 页面覆盖率提升至 100% 注册命令
- 文档与实现保持一致，减少维护漂移风险
- 后续可按需为高频命令补充 EXAMPLES/NOTES
