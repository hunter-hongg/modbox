# Agent Changelog

## 2026-9-12（工作区整理与完善）

- 整理工作区并完善：把上一批「已实现但悬在工作区」的成果归档提交，修正文档计数漂移，补齐状态文档，修复长期环境性测试失败。
  - **提交未跟踪成果**：`kill`/`od`/`pr`/`nslookup`/`sleep`/`sum` 六个命令的 bug 修复、9 个新测试文件（`test_{kill,od,pr,printenv,realpath,shred,sleep,sum,nslookup}.sh`，新增 72 条断言）、4 个 man 页（`modbox-{bc,man,setenforce,zcat}.1.md`）此前均未提交。
  - **计数修正**：README 命令计数 185→186（漏算 findmnt），CHANGELOG「All 182 commands」→186，`_command dispatch_` 说明行、`Current Status` 行同步。真实计数以 `REGISTER_COMMAND` 宏为准（186 个唯一名；`help` 显示 187 含 `[` 别名）。
  - **状态文档**：`.omo/plans/ausearch.md` 头部标注 implemented（代码早已完成但 12 个 todo 未勾）；`docs/superpowers/{STATUS,TODOS}-2026.md` 加 historical 说明（内容停留在 7 月，早已过时）。
  - **测试全绿**：`test_perf.sh` 的 3 条 `perf stat` 子进程退出码断言在 `perf_event_paranoid>1` 环境下必然失败（内核禁止非特权 perf_event_open 于 fork 后的子进程）。按仓库既有 root/EPERM 条件 idiom，改为检测 `perf_event_paranoid` 后 `pass "skipped (...)"`。全量测试由 3270 通过 / 3 失败 → **3273 通过 / 0 失败**。

## 2026-9-12

- 新增 `findmnt` 命令：按设备或挂载点查询文件系统（延续 mount/lsblk/df 存储工具链）。
  - **数据源**：优先 `/proc/self/mountinfo`（携带挂载 ID/Parent ID 关系，可据此构建挂载树），不可读时回退 `/proc/mounts`；测试通过 `MODBOX_MOUNTINFO` 环境变量注入固定挂载表，保证断言与宿主机布局无关。无新增依赖、无需特权。
  - **默认输出**：按 ID 父子关系渲染挂载树（`├─`/`└─`/`│` 前缀），列默认 `TARGET,SOURCE,FSTYPE,OPTIONS`。树由 ID 关系构建而非字符串前缀比较，避免把 `/varfoo` 错误嵌套到 `/var` 下（`target_matches` 要求边界字符为 `/`）。
  - **选项**：`-a/--all`、`-c/--canonicalize`、`-J/--json`、`-l/--list`、`-n/--noheadings`、`-o/--output`（可重复，接受大小写不敏感列名与 T/S/F/O 缩写，支持 MAJ:MIN/ROOT/ID/PARENT）、`-P/--pairs`、`-r/--raw`、`-R/--submounts`、`-S/--source`、`-T/--target`、`-t/--types`（可重复）、`-v/--invert`、`-V/--version`、`--help`；`-F/--fstab` 明确不支持并以退出码 2 拒绝。
  - **退出码**：0=成功；1=无匹配（静默、无 stdout，便于 shell 条件判断）/挂载表不可读/argtable3 解析失败（复用 `print_arg_errors`）；2=语义用法错误（未知列、`--fstab`）。
  - **实现要点**：`flatten()` 用显式栈迭代遍历（规避 clang-tidy `misc-no-recursion`）；`build_tree` 用索引树（`vector<vector<size_t>>` + roots）避免按值复制丢失后代节点（修复了初版只嵌套一层的 bug）。新命令 clang-tidy 0 告警（仅剩 argtable3 指针固有的 `misc-const-correctness`，与 base32/base64/cksum 等既有命令一致）。
  - 新增文件：`include/commands/findmnt.hpp`、`src/commands/findmnt.cpp`、`docs/man/modbox-findmnt.1.md`、`tests/test_findmnt.sh`（39 条断言）；`specs/findmnt_spec.md`；Makefile `MAN_SOURCES`、`registered_cmds.txt`、README 命令计数（184→185）各更新。
  - 全量测试 3270 通过、3 失败——3 项失败全部来自 `test_perf.sh`，为环境所致（`perf_event_paranoid=2` 禁止 `perf_event_open`，`perf stat true` 无法 fork/exec 子进程，rc=126），与本次改动无关；findmnt 自身测试 39/39 通过。

- 新增 `which` 与 `whereis` 两个命令定位工具（autopilot 全流程：spec → tickets → 实现 → code review → 提交）。
  - **`which`**：按 PATH 顺序查找可执行文件，默认每名首个匹配，`-a/--all` 打印全部；`--skip-dot`/`--skip-tilde` 过滤点/波浪号目录，`--show-dot`/`--show-tilde` 控制输出渲染；含斜杠的名字直接按路径判断可执行；未找到在 stderr 打印 `which: no NAME in (PATH...)`；退出码按 spec：0=全部解析、1=部分、2=全无或非法选项（不同于 GNU which 的 1/1 语义，用户明确选择遵循 spec）。
  - **`whereis`**：输出 `name: b… m… s…`（固定 b/m/s 顺序），`-b/-m/-s` 限制类别，`-B/-M/-S` 各自独立替换本类搜索根、`-f` 结束当前目录列表；`-u` 按 util-linux man page 语义（“unusual = 并非每个显式请求类别都恰有 1 条命中”）筛出；`-l` 打印有效查找路径。bin 根按 realpath 去重（`/bin`、`/sbin` 收敛到 `/usr/bin`），man 根展开为存在的 `man1..man9` 子目录；名字含斜杠时按 basename 标注与查找（`whereis /usr/bin/ls` → `ls: …`）。
  - 新增文件：`include/commands/{which,whereis}.hpp`、`src/commands/{which,whereis}.cpp`、`docs/man/modbox-{which,whereis}.1.md`、`tests/test_which.sh`（23 条）、`tests/test_whereis.sh`（20 条）；Makefile `MAN_SOURCES` 各注册一行。
  - **双轴 code review 修复**：Spec 轴发现并修复“含斜杠名字未按 basename 标注/查找”的偏差（新增 `name_label()` 并补测试）；修正三处 man page 陈述与实现不符（which 未知选项行为、无参数退出、whereis `-f` 语义）。Standards 轴（AGENTS.md）无违标。
  - 全量测试 3234 通过、0 失败（此前 3233）。已提交 6a9f7f7（仅提交本功能 9 个文件；README/specs 计数与上一会话未提交改动保持原样未纳入）。

## 2026-9-11

- 补齐项目缺口（本次会话）：修复三个长期缺测试的命令，补齐四个缺 man 页的命令，刷新过时的索引文件。
  - **Bug 修复（因补测试暴露）**：
    - `pr`：修复 stdin 无文件路径时 segfault（`mkstemp(const_cast<char*>("/tmp/pr_input_XXXXXX"))` 写入字符串字面量）；补齐 `-t`/`--no-header`、`-N` 数字列、`-d` 双空格、`--columns=N` 粘连；重写列布局为 GNU 列主序填充。
    - `od`：补齐 `-b`/`-c`/`-o`/`-d`/`-x` 快捷格式；修复 `-t x1` 解析（size 数字被误当第二个格式）；修复地址每行打印（GNU 格式每 16 字节一行、末尾打印偏移）；修复 hex address 宽度（`%06x`）；修复 signed decimal（`d` 为 signed）；修复 4-byte hex/decimal 宽度；补齐 `-An`/`-Ax` 粘连；补齐 `-tx1z` 解析；修复 `-tx1` 多次调用。
    - `kill`：修复 `-l` 后接信号参数（原忽略参数只打印全表）；修复 `-0`（空信号检查存活）；修复 `-s 0`；修复错误退出码（全为 0 → 改为 1）；修复 `kill -l TERM` 解析。
    - `nslookup`：修复 `--help` 因 `-t`/`domain` 必填而报错（改为 help 优先）；`-t` 改为可选默认 A。
    - `sleep`：错误退出码 0 → 1。
    - `sum`：文件不存在时错误退出码 0 → 1。
  - **补 man 页**：`modbox-bc`、`modbox-man`、`modbox-setenforce`、`modbox-zcat`；Makefile `MAN_SOURCES` 注册。
  - **刷新索引**：`registered_cmds.txt`（179→183，补漏的 `bc`/`man`/`setenforce`/`zcat`，说明 `[` 是 test 别名）；`man_pages.txt`（101→183）；`specs/missing_commands_overview.md` 从"仅余 pinky/stdbuf"更新为"无已知缺失命令"。
  - **补测试**：`test_kill.sh`（13 条）、`test_od.sh`（11 条）、`test_pr.sh`（11 条）、`test_printenv.sh`（7 条）、`test_realpath.sh`（8 条）、`test_shred.sh`（5 条）、`test_sleep.sh`（6 条）、`test_sum.sh`（7 条）、`test_nslookup.sh`（4 条，网络项可选跳过）。
  - 全量测试 3191 通过、0 失败（此前为 3065 通过）。命令总数 182（184 注册名含 `[`、`netcat` 别名），GNU coreutils 全覆盖。

## 2026-9-10

- 实现 `watch` 命令（autopilot 全流程：spec → tickets → 实现 → code review → 提交），完成 procps watch 子集：周期执行命令并显示输出，默认 `sh -c`（参数空格拼接）、`-x` 直接 execvp；stdout 经管道捕获，父进程 `poll()` + `waitpid(WNOHANG)` 并发排空（帧输出超过管道缓冲不会死锁），child stderr 继承不捕获；标题栏 `Every %.1fs: <cmd>` 右对齐 hostname/ctime 日期至 `ioctl(TIOCGWINSZ)` 宽度（回退 80），`-t` 关闭标题。选项：`-n/--interval`（默认 2.0，支持 s/m/h/d 后缀，>0 且 ≤1e6 上限校验）、`-e/--errexit`（透传命令退出码）、`-g/--chgexit`（首轮建立基线不退出）、`-b/--beep`（非零退出发 BEL）、`-d/--differences[=permanent]`（TTY 下反显变化行；可选值按 GNU getopt 语义仅粘连形式 `-dpermanent`/`--differences=permanent` 生效，空格形式 `permanent` 视为命令——经系统 GNU watch 4.x 实测对齐，spec/ticket 同步修订）。TTY 模式逐帧清屏并恢复光标；非 TTY 不输出任何 ANSI 序列（对 GNU watch 的刻意偏离，保证管道干净）。用法错误 exit 2、exec 失败 127。新增 `include/commands/watch.hpp`、`src/commands/watch.cpp`、`tests/test_watch.sh`（34 条）、`docs/man/modbox-watch.1.md`（MAN_SOURCES 注册）、`specs/watch_spec.md`、tickets 落 `.scratch/watch/`；README 计数 181→182、本地 registered_cmds.txt 记 179。经双轴 code review（Standards + Spec）修复 7 项：run_frame/print_title 改传 `const WATCHOptions*`（AGENTS.md 约定）、poll 失败路径补 waitpid 防 zombie、`-n` 缺参错误信息对齐 stdbuf、interval 上限拒绝、测试补 exit-code 断言与 `timeout` 包裹（防并行挂死）、test 15 改 `-b -e` 组合、registered_cmds/README 计数漂移。测试：test_watch.sh 34/34 通过；全量 3065 通过，余 2 项失败为 zcat 提交遗留 man 页缺失（非本次范围）；awk/base32 并行负载偶发误报、单独运行全绿。已提交 e46f44d。

## 2026-9-9

- 实现 `nc`（`netcat` 别名）命令（autopilot 全流程：spec → tickets → 实现 → code review → 提交），续接中断会话完成 TCP/UDP 双向中继工具：连接/监听双模式、`poll()` 双向中继、选项 `-c/-d/-i/-k/-p/-u/-v/-w/-z/-6`、负端口语法（host -8080）、粘连短选项（-w2/-p12345/-c<cmd>）、源端口绑定（-p，connect 前 bind）、IPv6 偏好（-6，AI_V4MAPPED|AI_ADDRCONFIG）。中断点的关键问题与修复：argtable3 3.x 下 `new_argc = arg_store.size()+1` 多计 1 导致 `arg_parse` 段错误（按 dispatcher 约定改为 size）；原实现 `host port` 双参数解析错误（arg_str0 每组最多 1 值）改为 host/port 双 location 分离解析；UDP 客户端 `recvfrom(MSG_PEEK)` 取对端地址阻塞死锁且覆盖 stdin 缓冲，改为 SO_PEERNAME 判定 connected 后统一走 plain `send()`/`recvfrom()`；-w 连接超时改用非阻塞 connect + poll POLLOUT（SO_RCVTIMEO 不约束 connect 本身）；-v 成功行 use-after-free（freeaddrinfo 之后才读解析 IP）改为 free 前捕获；成功行格式对齐 spec（`Connection to <host> <port> port [tcp/<service>] succeeded!`，-z 打印到 stdout / -v 到 stderr，仅一次）。新增 `include/commands/nc.hpp`、`src/commands/nc.cpp`、`tests/test_nc.sh`（29 条，含回环实时测试：TCP 往返、UDP、-k 多客户端、-i 限速、-c 执行、负端口、粘连选项、-w 超时）、`docs/man/modbox-nc.1.md` + `modbox-netcat.1.md`（MAN_SOURCES 注册）；README 命令列表与计数同步至 181（此前漂移 13 个已提交命令，上一会话留下的破损列表行亦修复）；SignalGuard RAII 安装/恢复 SIGINT、SIGPIPE 处理器。经双轴 code review（Standards + Spec）修复：-v 空 IP（UAF）、成功行缺 "port" 词、-w 无实时测试、-i 循环补 g_stop、-p 监听模式忽略行为写入 man 页、删除不可达的负端口值分支、SO_REUSEADDR 收紧为 -d/监听默认、删除 spec 外的 "Connection from client" 行。测试：test_nc.sh 29/29 通过；全量套件 3029 通过，余 2 项失败为 zcat 提交遗留的 man 页缺失（非本次范围）；awk/grep 在 8 路并行负载下偶发误报、单独运行全绿。已提交 bcc44bf。

## 2026-9-8

- 实现 `zcat` 命令（autopilot 全流程：spec → tickets → 实现 → code review → 提交），完成 GNU coreutils zcat 解压缩工具。实现方式复用现有 gzip 解压缩逻辑，直接输出到 stdout，不创建文件。支持多文件串联、stdin 读取、help/version 选项、错误处理。新增 `include/commands/zcat.hpp`、`src/commands/zcat.cpp`、`tests/test_zcat.sh`（12 条）。spec 落 `specs/zcat-spec.md`、tickets 落 `.scratch/zcat/`。全测试通过，已提交 feat(zcat): add gzip decompress to stdout command。

## 2026-9-7

- 实现 `column` 命令（autopilot 全流程：spec → tickets → 实现 → code review → 提交），完成 coreutils column 数据列化格式化工具。两种模式：fill 模式（默认，将输入行按 `cols = max(1,(width+1)/(max_width+1))`、`rows = ceil(num/cols)` 分布到尽可能多的列，逐列向下填充）与 table 模式（`-t`，字段对齐成列、单空格分隔、末列无尾部空白、不规则行补空且不留尾随空格）。选项覆盖：`-s` 自定义分隔符（默认空白串、支持 `\t`/`\n`/`\0`/`\\` 转义）、`-c` 宽度、`-o` 输出到文件、`-N` 列名（`-` 保留原值）、`-r`/`-R` 全部右对齐、`-C` 仅最右列右对齐、`-d` 分隔线、`-L` 缩进、`-e`/`-l` 多行单元格、`-H` 头部重复（每 25 数据行）、`-a`、`--help`/`--version`、多文件独立格式化后拼接、缺失文件报 stderr 并继续。新增 `include/commands/column.hpp`、`src/commands/column.cpp`（argtable3 + ArgTable + print_arg_errors；引入 `SepMode` 枚举并提取公共 `split_on` 消除三处重复分割循环）、`tests/test_column.sh`（30 条）、`docs/man/modbox-column.1.md`，Makefile MAN_SOURCES 注册。spec 落 `specs/column_spec.md`、tickets 落 `.scratch/column/issues/`（两者均被 .gitignore，未入版）。经双轴 code review 修复 1 项违标（`const ColumnOptions&` 按 AGENTS.md 约定改为 `const ColumnOptions*` 传参），并删除死字段 `full`、将魔数 25 命名为 `kHeaderRepeatEvery`、`right2_opt` 更名为 `table_right_opt`、`right_opt` 更名为 `right_justified_opt`，以及理顺 spec/man 不一致（fill 公式文本、多文件行为、`-H` 常量、`-c` 仅 fill 模式生效、`-s` 转义与 `-L` fill 模式补文档）。全测试 3006/0。已提交 0d5ffe4。

## 2026-9-6

- 实现 `pgrep` 命令（autopilot 全流程：spec → tickets → 实现 → code review → 提交），完成 procps pgrep 子集：枚举 /proc/[0-9]* 读 comm/cmdline（NUL→空格）/Uid，匹配支持 comm 子串（默认）、`-f` 全命令行（内核线程空 cmdline 回退 comm）、`-x` 精确、`-i` 忽略大小写、`-v` 反选、`-u` 用户名或数字 uid；输出格式 `-l`（PID comm）、`-a`（PID cmdline）、`-c`（计数）；退出码对齐 GNU pgrep（0 命中/1 无命中/2 用法错误）。经双轴 code review 修复 4 项：argtable3 + ArgTable + print_arg_errors 替换手写解析（AGENTS.md 约定）、内核线程 `-f` 回退死代码 bug（`if (!*target_cstr) continue` 先于回退赋值，导致空 cmdline 进程被排除）、合并重复的双匹配循环为单一 `matches` lambda + hits 向量单次遍历、`-h` 短选项补齐。最初与 pgrep 一同实现的 pkill 按用户决策放弃（`-f` 匹配会命中包含 pattern 的 shell 自身 cmdline，风险过高），相关文件全部删除，信号发送职责保留给既有 kill。新增 `docs/man/modbox-pgrep.1.md`、`tests/test_pgrep.sh`（15 条）、Makefile MAN_SOURCES/README/registered_cmds.txt 注册（命令数 174→175）。全测试 2973/0（test_perf.sh 3 条 pre-existing 失败，经 git stash 验证与本次无关）。已提交 1070027。

- 实现 `man` 命令（autopilot 全流程：spec → tickets → 实现 → code review → 提交），遵循 GNU man 规范读取 `docs/man/modbox-*.1.md`。支持页面显示、`-k/--apropos` 关键词搜索、`-f/--whatis` 一行描述、`-a/--all` 多匹配、`--help`/`--version`。新增 `include/commands/man.hpp`、`src/commands/man.cpp`，spec 落 `specs/man_command_spec.md`，tickets 落 `.scratch/man-command/issues/`。经 code review 移除注释、修复格式字符串安全问题。已提交 feat(man): implement man command following GNU conventions。

- 实现 grep GNU 兼容性扩展（autopilot 全流程：spec → tickets → 实现 → code review → 提交）：新增 `-A/-B/-C` 上下文、`-m` 最大匹配、`-q/-s` 静默/错误抑制、`-b/-Z` 字节偏移/空分隔、`--label`、 `--include/--exclude`、 `-d` 目录处理、`-P` 明确不支持提示，更新帮助文本与 man 文档，修复退出码 0/1/2 语义并修复 man 命令编译错误。新增 `docs/specs/grep-compat.md`、`include/commands/grep.hpp` 选项扩展、`src/commands/grep.cpp` 解析与逻辑、修复 `src/commands/man.cpp`。已提交 feat(grep): add GNU compatibility options and improve exit semantics。

## 2026-9-2

- 基于 handoff 文档续接实现 zip/unzip 命令收尾交付：修复 `tests/test_zip.sh` 将裸 `zip`/`unzip` 替换为 `$MODBOX` 前缀（此前误调系统 zip）、修 `cmp -s` 在 `[[ ]]` 内非法语法的两处语法错误、修 `-k` 无效 flag、修 entry filter 命令参数顺序、为目录创建加显式 `mkdir -p`。修 `src/commands/unzip.cpp` 默认覆写策略从"静默覆写"改为"跳过已存在"（仅 `-o` 覆写），对齐 spec。修 `src/commands/zip.cpp` 的 `compute_entry_name()`：绝对路径通过 `std::filesystem::relative` 相对 CWD 化、越界 `../` 时退化为 basename；新增 `compute_entry_name_relative()` 为 `-r` 递归场景保留目录内部路径层级，`process_dir()` 传顶层 root 避免层级丢失。新增 `tests/test_unzip.sh`（20 条覆盖 list/extract/-d/-t/-p/entry filter/-n/-o/-q/-v/错误/round-trip/interop）、`docs/man/modbox-zip.1.md` 与 `docs/man/modbox-unzip.1.md`、Makefile MAN_SOURCES 注册两条。全测试 2916/0 通过。

## 2026-9-1

- 实现 `file` 命令（从 spec 到落盘到测试到 code review 到修复），完成 6 个 ticket 全量交付。spec 与 ticket 因 `.gitignore` 存在落盘在 `specs/file_spec.md` 与 `.scratch/file-command/issues/`，未入版；GitHub 环境缺失未发 issue。commit `2cf2edd`：新增 `include/commands/file.hpp`（`FileOptions`）、`src/commands/file.cpp`（magic byte 分类器：ASCII/UTF-8 文本 + CRLF 检测、ELF 32/64-bit 含架构/字节序、gzip/ZIP、shebang "script executable"、symlink 默认跟随 + `--no-symlinks`、stdin via `-`、`-b`/`--brief`、多参数、`--help`）、`tests/test_file.sh`（26 条 + GNU parity 关键词对照）。commit `50a1151`：按代码审查修 10 项——argtable3 替换手写解析、删 `stdin_mode` 死字段、`const FileOptions*` 传参、内联 ELF 检查统一遍历、提取 `has_shebang()` 消除重复检测、删 dead `--no-symlinks` 分支、删 dead shebang else-if、shebang 描述对齐 spec "script executable"、parity 循环改用 portable for-pair（弃 bash associative array）、修 help 测试 grep stray-backslash 警告。全测试 2893/0 零回归。

- 记住约定：无 `gh` 环境时不再尝试 GitHub CLI，spec/ticket 落本地文件即可。

## 2026-08-31

- 实现 `tc` 只读流量控制检视命令：经 RTNETLINK 转储（RTM_GETQDISC/TCLASS/TFILTER）列举 qdisc/class/filter，支持 `show` 与 `dev <if>`/`parent <handle>` 范围、`-s/--stats`、`-d/--details`、`-json`、`-pretty`，输出兼容上游 `tc show`（`qdisc <kind> <handle>:` 前缀、root/ingress、refcnt），JSON 复用共享 json_stringifier；全程无需新依赖与特权的只读转储。新增 include/commands/tc.hpp、src/commands/tc.cpp、docs/man/modbox-tc.1.md、tests/test_tc.sh，注册进 CommandRegistry 并在 Makefile MAN_SOURCES、README 命令清单、registered_cmds.txt 加 tc 条目（ADR-014 覆盖测试对 166 命令仍绿）。代码审查发现并修复 5 处：TcOptions 改用 `const TcOptions*` 传参（AGENTS.md 约定）、未知设备报错（`no such device`、exit 1）、`-stats`/`-details` 长式别名、`--nope` 输出 `unrecognized option`、句柄格式 `8001:0`→`8001:`、filter 省略无意义 refcnt。另发现并绕过共享 json_stringifier 的 json_emit_uint64 逗号缺陷（tc 首个非末位使用者），改用手工分隔符。全测试 exit 0、tc 23 条测试全过。已提交 c206a74。

- 修复 mktemp 测试在并行测试套件（tests/run_tests.py，8 workers）下的偶发失败：根因为该 runner 为每个测试子进程导出 `TMPDIR`（如 `/tmp/modbox_test.XXXX`），而 `modbox mktemp` 会遵循 `$TMPDIR` 作为输出目录；但 tests/test_mktemp.sh 的 `-t` 断言把路径前缀硬编码为 `/tmp/`（`^/tmp/modboxtest`），文件实际落在 `$TMPDIR` 下时断言失败。改为断言生成文件 basename 以 `modboxtest` 开头（匹配 `mktemp -t` 的 "prefix" 语义），与文件实际落点无关。全测试由 2840 通过 / 1 失败 修复为 2841 通过 / 0 失败，仅改动测试、未动二进制。

## 2026-08-30

- 实现 `lspci` 与 `lsusb` 两个硬件枚举命令：读取 `/sys/bus/pci/devices/` 与 `/sys/bus/usb/devices/`（无需特权），从 `pci.ids`/`usb.ids` 解析厂商/设备名，PCI 类别码经内嵌表（源自 pci.ids "C" 段）映射为可读类别名。lspci 输出与系统 lspci 逐字节一致（26 设备，含 `(rev NN)` 后缀与地址排序），lsusb 为标准 `Bus X Device Y: ID vid:pid Vendor` 格式。两命令均支持 `--json`（字符串转义）、`-n`/`--no-name`、`--parse=<list>`、`-h`/`--help`、`--version`，lspci 另支持 `-e`/`--extended`。新增共享 `include/commands/ids_database.hpp` 解析器（按缩进深度区分厂商/设备行，跳过子系统/接口行与 C/AT/HID/BIAS 段头）、`lspci.hpp`/`lsusb.hpp`、两个 .cpp 与 man page，更新 Makefile MAN_SOURCES、registered_cmds.txt、tests/test_man_pages.sh。测试接缝 `MODBOX_SYSFS`/`MODBOX_IDS_DIR` 支持基于 fixture 的确定性测试（与主机拓扑/hwdata 无关），tests/test_lspci.sh + tests/test_lsusb.sh 共 25 条全通过。代码审查（对照真实 hwdata 实测）发现并修复 3 个 P1 正确性缺陷：ids 解析器原跳过全部前导 tab 致 2-tab 子系统行污染设备名（60 个真实名 + 1422 个伪键，验证 AMD 1002:1681 现正确解析为 Rembrandt [Radeon 680M]）、PCI 类别表自 0x0A 起整体错位一档（118 个已知码中 77 个错，0x0D 为占位 "Intel"，重建后验证 Bluetooth/SD-Host/Fibre Channel/Satellite 正确）、`--json` 未转义名称（真实库含 52 个带引号条目致 JSON 非法，改用 json_escape_string）。另修复：lspci 改为 collect-then-emit 使 JSON 逗号按实际输出计数、`return !vendor_id.empty()` 跳过 sysfs 目录下的杂散文件、`--extended=` strncmp 长度 10→11、补 `-h`、删除死代码、合并三处重复 hex 解析辅助函数；测试补 fixture 的引号转义/--extended/未知 --parse 字段覆盖并修正 lsusb 空设备分支。新头文件仅被 lspci/lsusb 包含，无跨命令回归（lscpu/lsblk 测试通过）。全测试唯一失败为 pre-existing 的 mpstat 缺 MAN_SOURCES 入口（源自未提交的上次 WIP，非本次范围）。已提交 e715f53。

## 2026-08-29

- 实现 `less` 命令：GNU 对齐的终端分页器，复用现有 `pager` 模块（单接缝），支持 j/k/space/b/g/G/q 导航、/ ? n N 正则搜索、-N 行号、-M 长提示符、+G/+N/+/pat 和 -p 启动定位、-i（smartcase）/-I（强制忽略大小写）-S/-E/-F/-X 开关、多文件分隔横幅、non-TTY 透传（退化为 cat）、退出码 0/1/2；新增 docs/man/modbox-less.1.md、tests/test_less.sh，扩展 tests/test_man_pages.sh 与 Makefile MAN_SOURCES。全测试 2387/0。修复 pager_run 启动搜索跳过第 1 行的 bug（`-p` 当 pattern 命中首行时未找到）。代码审查后按仓库约定将 PagerConfig 重命名为 PagerOptions 并按 `const XxxOptions*` 传参。clang-tidy 对 pager.cpp / less.cpp 清零。已提交 4 个 commits：f1854d8 / b77f9f8 / ff5bd45 / cf47a39。
- 实现 `pstree` 命令：读取 `/proc` 重建父→子层级，输出缩进 ASCII 进程树。支持 `-p`（括号内 PID）、`-a`（命令行参数，去重命令名）、`-u`（uid 切换内联）、`-s <pid>`（聚焦祖先链+子树）、`--help`、`-V/--version`；未知选项与 `-s` 无 PID 均报错退 1。复用 `ps.cpp` 的 `/proc` 读取约定，经 `REGISTER_COMMAND` 注册；新增 docs/man/modbox-pstree.1.md、tests/test_pstree.sh，扩展 Makefile MAN_SOURCES 与 tests/test_man_pages.sh。代码审查后修复三处真实 bug：`-a` 重复命令名、`-u` 根节点标注、`-s` 无 PID 静默 no-op。全测试 2407/0，已提交 db76303。
- 实现 SELinux 簇三个命令 restorecon / getsebool / setsebool，补全 modbox 的 SELinux 工具链（此前已有 getenforce/chcon/runcon/audit2allow/ausearch）。复用已链接的 libselinux 与既有 arg_util/version_util/command_macros 模式，零新依赖。restorecon 支持 -R/-v/-n/-F/-i，getsebool 列表/查询布尔值（对齐系统 getsebool 的 `name --> on|off` 格式），setsebool 运行时切换、-P 持久、批量 name=val；并把 getenforce/audit2allow 的错误打印机提取为 arg_util.hpp::print_arg_errors，统一 `unrecognized option`/`unexpected argument` 措辞。每命令含 man page（ADR-014）与 SELinux-不可用 SKIP 约定测试。代码审查后修复两处真实缺陷：getsebool 布尔名用 free() 而非 freecon()（错误释放 API）、restorecon -i 退出码递归/非递归路径不一致（统一为 -i 仅压 stderr、错误仍返非零）；另修 setsebool 旧形式 `-P` 静默丢持久化的 P1 bug。全测试 2813 通过 / 1 失败（pre-existing 的 mpstat 缺 MAN_SOURCES 入口，非本次范围）。已提交 01e124c / 77f4054。
- 修复测试门失效与 14 个失败用例：run_tests.sh 将 `__PASS__`/`__FAIL__` 打在同一行，汇总正则无法匹配 `__FAIL__`，`FAIL_COUNT` 恒为 0，套件谎报绿（实有 26 个 FAIL 行）。拆分计数到独立行后门生效。暴露的 14 个真实失败：7 个产品 bug（fold 无参读 stdin、join 默认 TAB 分隔符并支持 `-tX`/`--separator`、ls 彩色/图标路径误打 `classify_suffix` 函数指针而非本地 `suffix`、mount `--fake` 接受 `-O`、fd 无 PATTERN 退 2、runcon 无命令先报 `no command specified`、free `--si` 单位补 B）+ 7 个测试桩 bug（assert_cmd_pat 重复 `$MODBOX`、版本模式未转义括号、users 去重对齐系统、lsof `-p` 参数未拆分、lf 缺失 fixture、runcon 错误断言在 stdout、ls 排序取 basename）。全测试 2795/0，已提交 4e2d3b1。

## 2026-08-26

- 修复未知命令错误提示中 Usage 行显示问题：调用 `modbox <未知命令>` 时，Usage 行错误显示为 `Usage: <未知命令> [options]`（缺少 `<command>` 参数提示）；改为始终输出 `Usage: modbox <command> [options]`，使用户获得正确的帮助信息。

## 2026-08-25

- 完成 `ausearch` 审计日志搜索命令（依据 handoff 文档续接实现）：修复编译错误与 7 个深层 bug——argtable3 不支持多字符短选项改为长选项、`event_matches` 无条件返回 true 导致过滤失效、时间戳 serial 解析错位、syscall 表数据错误（从内核头文件 `asm/unistd_{64,32}.h` 重新生成 385+461 条）、输出 stamp 格式损坏、AVC 自由文本破坏字段解析、`-l` 短选项冲突；代码审查后按仓库约定改为 `const AusearchOptions*` 指针传递并补充 `-m ALL`、uid/gid 名称解析、`--uid-all`/`--gid-all` 值匹配；新增 55 条合成数据测试与 man page，全测试 2631 通过，已提交 6ca91fc。
- 按用户要求将 `.omo/` 工作文档目录（plans/specs/run-continuation，22 文件）纳入 git 跟踪并记住此约定，已提交 3e3f72c。

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

- 修复 man page 文档覆盖缺口与测试盲区：注册命令 free/iostat/mpstat/vmstat 缺失 docs/man/* 及 Makefile MAN_SOURCES 条目；新增 4 个 man page，更新 Makefile，将 tests/test_man_pages.sh 改为基于 registered_cmds.txt 的注册表驱动全覆盖断言（缺失 man page / MAN_SOURCES 缺项直接 FAIL），防止未来新增命令时遗漏文档。配合文档同步重生成 registered_cmds.txt 与 README inline 清单后，全测试 2408/0 通过（无回归）。

## 2026-9-5

- 实现 tar 命令并修复 handoff 中列出的全部 10 项关键缺陷：mtime 刷新前 fflush、递归 walk_dir、路径穿越校验、--xz/--zstd 长选项映射、pax reader 去除尾随换行、pax writer 长度自稳定计算、field_put uid/gid 阈值、parse_args 重复赋值/缺参错误、移除未使用 argtable3/bzlib。新增 docs/man/modbox-tar.1.md、docs/man/modbox-file.1.md、Makefile MAN_SOURCES 更新、tests/test_tar.sh 覆盖创建/列表/提取/递归/mtime/mode/压缩/pax/管道/GNU 互操作/错误。刷新 registered_cmds.txt 至 174 条、README 命令数 163→174、CHANGELOG 补充。经双轴 code-review 后补修未知长选项静默、pax mtime 高精度格式、zstd 压缩与 pax 测试。全测试通过，已提交 a2cb803 与 a2562a8。

- 补齐 pinky 与 stdbuf 两个缺失命令：pinky 基于 utmp_util 读取 /var/run/utmp，新增 idle 时间计算（分钟/小时/天）、-l/-b/-f/-i/-p/-s/-q 选项及 help/version；stdbuf 改为在 fork 前设置 _STDBUF_I/_STDBUF_O/_STDBUF_E 环境变量，支持 -i/-o/-e 及 --input/--output/--error，支持 -oL/-i0 等粘连写法，错误路径统一返回 1。顺带修复 tar.cpp do_create 的指针/引用不一致编译错误。经双轴 code-review 修复 dangling temporary、粘连选项解析、错误返回码、idle 天显示、-s 行为。Spec 落在 specs/pinky-stdbuf-spec.md、tickets 落在 .scratch/pinky-stdbuf/issues/01~04-*.md（均 gitignore）。全测试 2962/0 通过，已提交 09e06f7。
