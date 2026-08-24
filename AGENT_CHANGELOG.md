# Agent Changelog

## 2026-08-24

- 实现 `free` 命令：读取 `/proc/meminfo` 输出 total/used/free/available/shared/buff/cache/swap 统计，支持 `-h` 人类可读、`--si` SI 单位、`-t` 总计行、`-o` 旧格式、`--json` JSON 输出、`--help`/`--version`，含 14 条集成测试；修复代码审查发现的违标（int64_t 类型、FreeOptions struct、合并 format_human/format_si 为 format_size、修正 used 计算、修复 help 文本重复 -h）。
- 精简 zstd/xz 压缩模块：新增 `include/commands/compress_util.hpp` 共享 `read_all/ends_with/strip_suffix/write_output_file/print_ratio`，zstd 与 xz 移除约 300 行重复实现。
- 扩展 man page 覆盖至 155/138 命令：新增 56 个 man page（docs/man/modbox-*.1.md），更新 Makefile MAN_SOURCES，扩展 tests/test_man_pages.sh 测试覆盖率。

## 2026-08-23

- 实现 iostat/vmstat/mpstat 三个系统监控命令：iostat 输出 CPU 与磁盘 I/O 统计（支持 -J/--json、-S 单位缩放），vmstat 输出进程/内存/Swap/IO/CPU 统计（支持 -a 活跃内存列、-d 磁盘统计、delay/count 重复采样），mpstat 输出逐核 CPU 利用率（支持 --all 逐核显示、--json）。读取 /proc/stat、/proc/meminfo、/proc/diskstats、/proc/loadavg、/proc/uptime，全测试 2562 通过。

## 2026-08-23

- 实现 jq 命令并修复标准合规：新增 src/commands/jq.cpp、include/commands/jq.hpp、docs/man/modbox-jq.1.md 与 tests/test_jq.sh，完成 issue #100 Spec，实现最小 JSON 解析器与字段/数组访问、-r/-c/-s 输出模式、stdin/file 支持与 help/version/man page。随后修复代码评审发现的违标：改为 argtable3 参数解析、移除未用 compare 辅助函数、去重转义逻辑、重命名变量、引入 OutputOptions 结构体、改用 STL I/O，全部测试通过并已提交。
- 修复 jq 无限循环：修正 for 循环增量与命名空间闭合，消除 bad_alloc 与超时问题。
- 精简 zstd/xz 压缩模块：新增 `include/commands/compress_util.hpp` 共享 `read_all/ends_with/strip_suffix/write_output_file/print_ratio`，zstd 与 xz 移除约 300 行重复实现，统一工具函数；修复 xz `-0` 级别解析，支持多位数字与组合标志；清理未用 prog 参数；全测试 2487 通过。

## 2026-8-19

- 为 df/du/env/kill/id/timeout/truncate/hostname/umask/whoami 十个高频命令新增 man page，扩展 `docs/man/` 10 个源文件，注册进 Makefile MAN_SOURCES，新增 tests/test_man_pages.sh 覆盖存在性/构建/内容/注册验证。覆盖率从 71/157（45%）提升至 81/157（51%）。全测试 2446 通过。
- 实现 `nslookup` 命令：基于原始 UDP 的 DNS 查询，支持 A/AAAA/MX/NS/CNAME/SOA/PTR/TXT/SRV 九种记录类型，自动将 PTR 查询中的 IP 地址转换为反向 DNS 格式（如 8.8.8.8 → 8.8.8.8.in-addr.arpa），支持自定义 DNS 服务器（`-s`）及 `-h`/`-V` 帮助/版本。全测试 2405 通过，已发布 spec 至 GitHub issue #84。

## 2026-8-17

- 实现 `lsof` 命令，读取 `/proc` 文件系统枚举进程打开的文件描述符，支持 `-p`/位置 PID、`-c`/`-u`/`-t`/`-d` 过滤、`-a` AND 逻辑、`-i` 网络过滤、`--json`/`-F` 输出格式、`-r` 重复、`-R` 链接计数、socket 地址解析（`/proc/net/tcp,udp,unix`），含 24 条集成测试和 man page。lint 通过，2344 全测试通过。

## 2026-8-14

- 实现 GitHub issue #62：为已实现的 chattr/chcon/chgrp/chroot 命令新增四个 pandoc man page，逐项对照 src/commands/*.cpp 的 argtable 校验选项（无虚构 flag，chattr 实际用 `-v` 而非 issue 中误写的 `-V`），注册进 Makefile 的 MAN_SOURCES，新增 tests/test_man_issue62.sh 覆盖存在性/MAN_SOURCES/渲染/必要章节（28/28 通过），全测试 2377 通过，已提交 69afb7a。
- 实现 GitHub issue #64：新增 `ip` 和 `ss` 两个网络诊断命令。`ip` 支持 `addr`/`link`/`route` 子命令及 `-4`/`-6`/`-s` 过滤，数据源为 `getifaddrs`、`/sys/class/net/`、`/proc/net/route`；`ss` 支持 TCP/UDP 查看、`-t`/`-u`/`-l`/`-a`/`-n`/`-4`/`-6` 标志及状态过滤，数据源为 `/proc/net/tcp{,6}` 和 `/proc/net/udp{,6}`。新增 tests/test_ip.sh（12 条）和 tests/test_ss.sh（10 条），全测试 2400 通过，已提交 c29d26e。
