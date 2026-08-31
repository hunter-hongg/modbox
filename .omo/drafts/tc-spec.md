# ModBox Implementation Spec: tc Command

> **Reference**: Linux `tc` from iproute2 (network traffic control). This spec covers a pragmatic, **read-only introspection** subset appropriate for a BusyBox-style multi-call binary — listing qdiscs, classes, and filters already configured on the system, without the privileged configuration surface of upstream `tc`.

## Problem Statement

modbox already ships a family of network diagnostics and control utilities — `ip`, `ss`, `arping`, `ping`, `tcpdump` — but has no way to inspect the **traffic control** state of a Linux host. On minimal containers or embedded systems that ship modbox instead of full iproute2, an operator who wants to see which qdiscs, classes, and filters are attached to an interface must fall back to an external `tc` binary, defeating the purpose of a self-contained toolkit. There is currently no modbox command that answers the basic question "what traffic-control policy is in effect on this interface?"

## Solution

Implement `tc` as a modbox command that introspects the kernel's traffic-control configuration and prints it in a format compatible with upstream `tc show`. It supports three read-only object types — **qdisc** (queuing discipline), **class**, and **filter** — each queryable per interface or across all interfaces. It mirrors the established companion commands (`ip`, `ss`) by reading directly from kernel interfaces (here, the RTNETLINK family) with **no new external dependency**, and it reuses modbox's existing JSON-output and help/version conventions so it slots into the multi-call binary without special-casing.

## User Stories

1. As a sysadmin, I want to run `tc qdisc show` so that I can see every queuing discipline attached to every interface on the host.
2. As a sysadmin, I want to run `tc qdisc show dev eth0` so that I can limit the listing to a single interface.
3. As a sysadmin, I want to run `tc class show dev eth0` so that I can see the traffic classes defined on an interface.
4. As a sysadmin, I want to run `tc filter show dev eth0` so that I can see the packet filters (classification rules) attached to an interface.
5. As a sysadmin, I want to run `tc qdisc show` with no interface so that I get the default behavior of listing all interfaces, matching upstream `tc`.
6. As a sysadmin, I want `tc -s qdisc show` (or `tc qdisc show -s`) so that I can see per-qdisc statistics: bytes sent, packets, drops, overlimits, requeues, and backlog.
7. As a sysadmin, I want `tc -d qdisc show` so that I can see detailed qdisc parameters (e.g. limit, flows, quantum, target, interval for fq_codel).
8. As a developer, I want `tc -json qdisc show` so that I can get machine-readable output for scripting and tooling.
9. As a developer, I want `tc -pretty qdisc show` combined with `-json` so that the JSON is indented and human-readable.
10. As a script writer, I want the JSON output to be parseable by `modbox jq` so that I can extract fields without writing a custom parser.
11. As a sysadmin, I want `tc qdisc show` to print a recognized `qdisc <kind> <handle>:` prefix (e.g. `qdisc noqueue 0:`) so that existing log scrapers and habits keep working.
12. As a sysadmin, I want each qdisc line to include `dev <ifname>`, `root`/`ingress`/`parent`, and `refcnt` so that I can understand attachment and lifecycle at a glance.
13. As a script writer, I want `tc --help` to display concise usage information and exit 0.
14. As a script writer, I want `tc --version` to display version information consistent with other modbox commands and exit 0.
15. As a script writer, I want `tc qdisc help` (and `tc class help`, `tc filter help`) to show subcommand-specific usage and exit 0.
16. As a script writer, I want `tc` with no subcommand to print usage to stderr and exit non-zero, mirroring `ip`'s behavior.
17. As a script writer, I want an unrecognized subcommand (e.g. `tc bogus`) to print an error to stderr and exit non-zero.
18. As a script writer, I want an unrecognized option (e.g. `tc --nope`) to print `unrecognized option` to stderr and exit non-zero, matching the standard modbox argtable3 error wording.
19. As a sysadmin, I want `tc qdisc show dev nonexistent0` to produce a clear error (unknown device) rather than a crash.
20. As a developer, I want `tc -stats qdisc show` to be accepted as a long-form alias of `-s`.
21. As a developer, I want `tc -details qdisc show` to be accepted as a long-form alias of `-d`.
22. As a sysadmin, I want the default listing to order interfaces consistently (e.g. alphabetically / by ifindex) so that output is stable across runs.
23. As a sysadmin, I want `tc filter show dev eth0 parent 1:0` to scope filters to a given parent handle when I supply one.
24. As a developer, I want the command to register with the existing `REGISTER_COMMAND` dispatch so that `modbox tc` and the `tc` symlink both work, consistent with every other modbox command.
25. As a documentation maintainer, I want a man page (`modbox-tc.1.md`), a `Makefile` `MAN_SOURCES` entry, and a `registered_cmds.txt` entry created for `tc`, so that it satisfies the project's man-page coverage requirement (ADR-014).
26. As a test author, I want deterministic argument-validation and help/version tests that need no network or privileges, so that the CI suite stays green without a special environment.
27. As a developer, I want read-only introspection to require no elevated capabilities, so that the command works unprivileged (kernel dump sockets for RTM_GETQDISC/RTM_GETTCLASS/RTM_GETTFILTER do not require CAP_NET_ADMIN).
28. As a sysadmin, I want `tc -json qdisc show` to include the same fields as the text view (kind, handle, dev, parent, refcnt, stats) so that tooling does not lose information vs. the human view.
29. As a developer, I want the command to reuse modbox's shared version/help and error-printing helpers so that wording stays consistent with `ip`, `ss`, and the SELinux cluster.
30. As a packager, I want no new `PKGS` entry (no libmnl/libselinux/libcap additions) so that the dependency footprint stays minimal, matching the rest of modbox.

## Implementation Decisions

### Scope and subcommand shape

The command exposes three introspection object types — `qdisc`, `class`, `filter` — each accepting the `show` action and an optional `dev <ifname>` scope. Upstream `tc` also supports `add`/`del`/`change`/`replace` and a large configuration vocabulary; those are explicitly **out of scope** for v1 (see Out of Scope). The v1 surface is the read-only half only, which keeps the command unprivileged and dramatically shrinks the option space.

### Data source: RTNETLINK, no new library

The kernel exposes traffic-control state via the routing netlink family:
- qdiscs via `RTM_GETQDISC`,
- classes via `RTM_GETTCLASS`,
- filters via `RTM_GETTFILTER`.

These are read by opening an `AF_NETLINK`/`NETLINK_ROUTE` socket and issuing dump requests, the same mechanism iproute2 uses. modbox takes a dependency on the kernel headers only (`<linux/netlink.h>`, `<linux/rtnetlink.h>`, `<linux/pkt_sched.h>`) — **no new `PKGS` entry** is required, consistent with the project's "no new dependency" principle and the companion `ip`/`ss` commands that already read kernel interfaces directly. This avoids pulling in libmnl, which upstream `tc` uses but which modbox does not otherwise need.

### Output model

A netlink dump reader parses each response message into a typed record (kind, handle, parent, bound device index, refcnt, and — when stats are requested — the embedded statistics: bytes, packets, drops, overlimits, requeues, backlog, qlen). A single output formatter renders those records in two modes:
- **Human-readable text**: line-oriented, compatible with the upstream `tc show` prefix format (`qdisc <kind> <handle>: dev <if> ...`), extended by `-s` (statistics block) and `-d` (detailed parameters).
- **JSON**: enabled by `-json`, structured as an array of objects carrying the same fields as the text view, with `-pretty` opting into indented output. This reuses the JSON conventions already established across modbox commands (`free`, `iostat`/`mpstat`/`vmstat`, `lspci`/`lsusb`, etc.) rather than inventing a new shape.

### Interface selection and ordering

When no `dev` is given, the command enumerates all network interfaces (via the same interface-list source `ip`/`ss` already use) and dumps traffic-control state for each, so the default behavior matches upstream `tc` (all interfaces). Output is ordered deterministically by interface name/index so repeated runs are stable.

### Argument handling and errors

Options follow the project's argtable3 convention. Global flags (`-s`/`--stats`, `-d`/`--details`, `-json`, `-pretty`, `-h`/`--help`, `--version`) are parsed up front; the subcommand (`qdisc`/`class`/`filter`) and action (`show`) follow. Unknown options emit the standard `unrecognized option '...'` wording via the shared error helper. Missing subcommand or unknown subcommand prints usage to stderr and exits non-zero, mirroring `ip`. Unknown device names are reported as a clear error rather than aborting.

### Registration and documentation obligations

The command is registered through the existing `REGISTER_COMMAND` dispatch (no change to the dispatch mechanism itself). Per ADR-014, adding the command requires a man page source, a `Makefile` `MAN_SOURCES` entry, and an update to the command registry list so the man-page coverage test (`tests/test_man_pages.sh`) stays green. This keeps `tc` at parity with the 154+ already-documented commands.

## Testing Decisions

### What makes a good test

Tests should assert **external, observable behavior** — the emitted text/JSON and the exit code — and avoid coupling to internal parsing of netlink messages. Because traffic-control state depends on the host kernel, assertions must be tolerant of the host's actual configuration (e.g. assert the *presence* of expected tokens like `qdisc`, `dev`, and `root`/`refcnt`, not an exact byte-for-byte qdisc list), while argument-validation and help/version tests are fully deterministic.

### Modules tested

- The `tc` command entry point: help, version, subcommand dispatch, error paths.
- The `qdisc` object: `show` across all interfaces and scoped to a known interface (`lo` is always present and reliably carries a `noqueue` qdisc), plus `-s` statistics and `-d` details.
- The `class` and `filter` objects: `show` executes and produces well-formed output (these are frequently empty on a default host, so tests assert the command succeeds and either lists entries or reports none cleanly rather than asserting specific content).
- The JSON path: `-json` output is valid JSON and round-trips through `modbox jq`.

### Prior art / seams (prefer existing)

- **Highest seam — existing test framework**: new `tests/test_tc.sh` uses the shared `framework.sh` helpers already exercised by `tests/test_ip.sh` and `tests/test_ss.sh`: `assert_cmd`, `assert_cmd_pat`, `assert_cmd_not_pat`, `assert_cmd_pat_stderr`. No new harness is introduced.
- **Argument-validation seam (zero dependencies, deterministic)**: `--help`, `--version`, unknown subcommand, unknown option, missing subcommand, unknown device — all assert stderr wording and/or non-zero exit without touching the network or kernel.
- **Live-kernel introspection seam (no privileges required)**: `tc qdisc show` and `tc qdisc show dev lo` run against the real kernel via netlink dumps, which need no capabilities for read. Assertions check for tokens such as `qdisc`, `dev lo`, and `root`/`refcnt`, plus the `-s` statistics keywords (`Sent`, `bytes`, `pkt`, `dropped`).
- **JSON seam (reuses existing command)**: `tc -json qdisc show | modbox jq` — leverages the in-repo `jq` command to validate structure, avoiding a separate JSON parser in the test.
- **Man-page coverage seam (ADR-014)**: `tests/test_man_pages.sh` already enforces that every registered command has a man page source and `MAN_SOURCES` entry; adding `tc` extends that registry-driven check rather than adding a bespoke test.

## Out of Scope

- **Any mutating operation**: `tc qdisc/class/filter add`, `del`, `change`, `replace`, `link`, `ingress`/`egress` configuration. v1 is introspection only.
- **Qdisc/class/filter configuration parameters**: specifying kinds with options (e.g. `htb`, `tbf`, `netem` parameters), creating hierarchy, setting rates/ceil/bursts.
- **Actions**: `gact`, `mirred`, `police`, `skbedit`, etc.
- **`tc monitor`** (event streaming) and **`tc exec`**.
- **`tc class show` hierarchical tree rendering** beyond a flat, handle-ordered listing.
- **Ingress qdisc configuration** beyond introspecting an existing `ingress`/clsact qdisc that the kernel already reports.
- **Hardware-offload (`offload`) specifics** beyond whatever the kernel dump already reports.
- **`tc -b` batch files** and **`tc -force`** semantics.
- **Replacing upstream `tc` for control-plane use** — this command is a read-only inspector, not a configuration tool.

## Further Notes

- The reference `tc` is part of iproute2 and is backed by libmnl + a large configuration grammar. This spec targets the read-only ~20% of usage (the `show` family) that answers "what is configured," which is also the part usable without `CAP_NET_ADMIN`.
- Read-only RTNETLINK dumps (`RTM_GETQDISC`/`RTM_GETTCLASS`/`RTM_GETTFILTER`) do **not** require elevated capabilities on Linux, so the command works unprivileged — same spirit as `ip`/`ss` reading `/proc` and `getifaddrs`.
- The command is invoked as `modbox tc` or directly as a `tc` symlink, consistent with modbox's multi-call binary architecture.
- JSON output deliberately mirrors the text view's field set so downstream tooling loses no information versus the human view; `-pretty` is only meaningful alongside `-json`.
- Upstream `tc` accepts global flags both before and after the subcommand (`tc -s qdisc show` and `tc qdisc show -s`); the implementation should accept the common orderings rather than a single rigid position, since that is what users expect from muscle memory.
- Keeping the v1 surface to introspection makes the bulk of testing deterministic (argument validation) or privilege-free (live kernel dumps), which protects CI stability compared to a configuration-capable `tc`.
