# Draft: add-tcpdump

- intent: clear
- review_required: false
- classification: Standard（5 模块特性；全部实现决策已由 spec #102 锁定）
- status: approved（用户于 2026-08-26 批准："TDD；批准"）
- slug: add-tcpdump
- plan_path: .omo/plans/add-tcpdump.md
- pending-action: write .omo/plans/add-tcpdump.md
- pending_action_policy: { review_required: "write and review .omo/plans/add-tcpdump.md", otherwise: "write .omo/plans/add-tcpdump.md" }
- source_spec: https://github.com/hunter-hongg/modbox/issues/102
- source_tickets: #103 (scaffolding), #106 (pcap-decode), #107 (filter-display), #108 (pcap-write), #109 (live-capture)
- note: 本环境无 shell 工具，scaffold-plan.mjs 无法直接执行；draft/plan 头部按 references/full-workflow.md 模板 + 仓库地面真值 .omo/plans/ausearch.md 逐字复现。

## Components ledger (TOPOLOGY LOCK)

| id | 一句话结果 | status | evidence |
|---|---|---|---|
| scaffolding | `modbox tcpdump` 注册成功；`--help`/`--version` 可用；man page 存在并注册；README 更新 | pending | ticket #103 |
| pcap-decode | `-r FILE` 读 classic pcap（双端序），解码 Ethernet II/ARP/IPv4/IPv6/TCP/UDP/ICMP/ICMPv6，逐包单行输出 | pending | ticket #106 |
| filter-display | `-f` 过滤器子集（host/net/port/proto/src/dst + and/or/not + 括号，用户态求值）；`-q/-v/-e/-x/-tt/-s/-n` 显示选项 | pending | ticket #107 |
| pcap-write | `-w FILE` 写 classic pcap（Wireshark 兼容）；`-r`→`-w`→`-r` 往返等价 | pending | ticket #108 |
| live-capture | AF_PACKET+SOCK_RAW 实时抓包（无 libpcap）；`-i`/`-i any`/默认 any；`-c N`；SIGINT/SIGTERM 统计摘要；EPERM/未知接口优雅报错 | pending | ticket #109 |

## Decisions ledger

- D1: 单计划文件覆盖全部 5 ticket（one request -> one plan）。波次：W1 scaffolding → W2 pcap-decode → W3 filter-display ∥ pcap-write（并行）→ W4 live-capture。
- D2: 测试策略（用户已拍板）：**TDD @ bash CLI 层** —— 每个 todo 先在 tests/test_tcpdump.sh 增加会失败的断言（red，`bash tests/test_tcpdump.sh` 可见 FAIL）→ 实现 → `make && bash tests/test_tcpdump.sh` 零 FAIL（green）；fixture pcap 在测试脚本内用 xxd -r -p 字节序列生成；无 C++ 单测；实时抓包只测 CLI 表面 + 错误路径（dig 先例）；CI 零网络零 root。
- D3: 提交策略：Conventional Commits 英文，一个 todo 一个 commit（`feat(tcpdump): ...` / `docs(tcpdump): ...`）。
- D4: 零新依赖（spec 锁定）：AF_PACKET + SOCK_RAW，不引 libpcap，不实现 BPF 编译器。
- D5: dirty_worktree 风险（子代理报告 2026-08-26）：工作区存在与本任务无关的既有改动（.omo/run-continuation/*.json、AGENT_CHANGELOG.md、src/main.cpp）。执行者不得触碰/回滚它们，commit 只 stage tcpdump 相关路径。
- D6: 计划任务结构：4 波 × 14 个实现 todo（每个含 references + acceptance + 快乐/失败 QA + commit）+ F1-F4 最终验证波。

## Approval gate

- status: awaiting-review (user 于 2026-08-26 选择 "先高精度评审"；由 CLEAR 路径触发 dual review)
- plan: **complete** — `.omo/plans/add-tcpdump.md` 已写入（4 波 × 15 TDD todos + F1-F4，头部逐字复现脚本模板，结构自检通过：19 个 column-0 任务行，章节顺序符合模板）
- Metis 缺口分析：已运行（后台 ses_fc2239893ffeREwpRQYk5UxCrv），17 条 gap 全部处置 —— 真问题已折入计划：GAP1 退出码（usage/parse→2，仓库证据 ping.cpp:105/arping.cpp:75/dig.cpp:522/jq.cpp:207 + 各测试断言）、GAP12 ICMPv6 133/134/135/136 类型名（原计划写反，已修正）、GAP3 -r 摘要（补充 real-tcpdump 兼容性依据）、GAP2 -c 双路径 kept 语义、GAP4 `-i any` 字符串比较不走 if_nametoindex、GAP6/7/17 writer 端序/往返 byte-identical 定义/stdin+-w 支持、GAP10 root 不启动 no-args 测试、GAP13 用 $MY_UID、GAP14 full decode pass 定义、GAP15 SIGINT mid-write 文件完整性；man 产物路径修正为 build/man/modbox-tcpdump.1（Makefile patsubst 去 .md）。误读类（-nn、snaplen 默认、cannot open 字符串、大小写不敏感、GAP8/9/11/16）已在计划中覆盖，无需改动。
- review: **已完成**（2026-08-26，两轮双评通过）
  - Momus (ses_fbce3d509ffeRoX8o5l0V2Y5zs): APPROVED, HIGH 信心, issues=[], qa_coverage_gap=ALL_TODO_QA_VERIFIABLE
  - Oracle (ses_fc1d3c534ffeR3dNdrAvrODJpw): APPROVED_WITH_NOTES, HIGH 信心, 2 MINOR（T1 `-v`/`-V` 注意不混用；F4 显式列出 7 个 in-scope 路径），零 BLOCKING
- pending-action: handoff + 询问用户
