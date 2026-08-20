% MODBOX-PERF(1) modbox | User Commands
% modbox project
% 2026-08-19

# NAME

modbox-perf - measure performance events

# SYNOPSIS

**modbox perf** [*OPTION*]... **stat** [*OPTION*]... **COMMAND** [*ARG*]...
**modbox perf** **list** [*hw*|*sw*]
**modbox perf** **record**
**modbox perf** **report**
**modbox perf** **annotate**

# DESCRIPTION

**perf** is a performance monitoring tool that measures hardware and software
performance events for a given command. It uses Linux's **perf_event_open**
syscall when available, with graceful fallback to **getrusage**(2) for software
events when the syscall is unavailable.

The primary subcommand is **stat**, which runs **COMMAND** and reports counters
such as CPU cycles, instructions, cache misses, and branch misses.

Other subcommands (**record**, **report**, **annotate**, **sched**, **top**)
are provided as stubs that indicate the command exists but is not yet
implemented.

# SUBCOMMANDS

**stat** [*OPTIONS*]... **COMMAND** [*ARGS*]...
:   Run **COMMAND** and measure performance events.

**list** [*hw*|*sw*]
:   List available performance events. With **hw**, show only hardware events.
    With **sw**, show only software events. Without a filter, show all events.

**record**, **report**, **annotate**, **sched**, **top**
:   Not implemented. These stubs exist to indicate future functionality.

# STAT OPTIONS

**-e** *events*
:   Comma-separated list of events to count. Examples:
    **-e** cycles,**-e** cycles,cache-misses. May be specified multiple times.
    Supported event names include those listed under **EVENTS**.

**-I** *interval*
:   Output statistics every *interval* milliseconds. The final output after
    the command exits shows accumulated totals.

**-o** *file*
:   Write output to *file* in addition to stderr.

**--format** *list*
:   Comma-separated list of event names to display. Only events in this list
    are shown, in the order given. Events not in the list are omitted.

**--csv**
:   Output in CSV (comma-separated values) format with a header row.
    Suitable for importing into spreadsheets or data processing pipelines.

**--null**
:   Skip the "Performance counter stats for..." header line.

**--all-cpus**
:   Count events on all CPUs instead of just the thread's own CPU.

**--no-merge**
:   When used with **--all-cpus**, show per-CPU breakdown instead of merged
    totals.

**--repeat** *N*
:   Repeat the command *N* times and show an average summary at the end.

# EVENTS

## Hardware events

These events require **perf_event_open** and measure CPU microarchitectural
counters. They may show **N/A** when the syscall is unavailable.

**cycles**
:   CPU clock cycles.

**instructions**
:   Instructions retired.

**cache-references**
:   Cache references (loads + stores).

**cache-misses**
:   Cache misses.

**branch-instructions**
:   Branch instructions taken.

**branch-misses**
:   Branch mispredictions.

**stalled-cycles-frontend**
:   Cycles stalled waiting for instruction fetch.

**stalled-cycles-backend**
:   Cycles stalled waiting for execution units.

## Software events

These events are derived from **getrusage**(2) and always work.

**task-clock**
:   Elapsed CPU time in milliseconds (user + system).

**cpu-clock**
:   Same as task-clock; measures wall-clock time multiplied by number of CPUs.

**page-faults**
:   Total page faults (major + minor).

**minor-faults**
:   Minor page faults (no disk I/O required).

**major-faults**
:   Major page faults (disk I/O required).

**context-switches**
:   Context switches (voluntary + involuntary).

**cpu-migrations**
:   Number of times the process was migrated to another CPU.

# OUTPUT FORMATS

## Default (human-readable)

Events are displayed in a table with the event name left-aligned and the
count right-aligned. Large numbers use SI suffixes (K, M, G, T). When both
**cycles** and **instructions** are present, an IPC (instructions per cycle)
ratio is shown as a comment.

```
 Performance counter stats for 'sleep 1':

         998.723      task-clock (msec)
           1.234G      cycles
           654.32M      instructions              #    0.53  IPC
              10.00      cache-references
               3.00      cache-misses              #    0.030% of all cache refs

   1.001234 seconds time elapsed
```

## CSV

With **--csv**, output is pipe-delimited with a header row:

```
event,count
task-clock,998.723
cycles,1234567890
instructions,654321000
elapsed,1.001234
```

# EXIT STATUS

**0**
:   Command succeeded.

**1**
:   Command failed (propagated from the child process).

**2**
:   Invalid options or missing required arguments.

**127**
:   Command not found (propagated from exec).

# EXAMPLES

```bash
# Basic usage
modbox perf stat true

# Count specific events
modbox perf stat -e cycles,instructions sleep 1

# CSV output for data processing
modbox perf stat --csv -e cycles instructions false

# Repeat 3 times and average
modbox perf stat --repeat 3 sleep 0.1

# Write to file
modbox perf stat -o results.txt sleep 1

# List all events
modbox perf list

# List only hardware events
modbox perf list hw

# List only software events
modbox perf list sw

# Format filter: show only selected events
modbox perf stat --format cycles,instructions true

# Skip header line
modbox perf stat --null true
```

# SEE ALSO

**modbox-time**(1), **modbox**(1)
