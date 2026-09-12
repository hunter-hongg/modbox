% MODBOX-CHRT(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-chrt - Show or change real-time scheduling attributes of a process

# SYNOPSIS

**modbox chrt** [*options*] [*priority*] *command* [*argument*...]

**modbox chrt** **--pid** *policy-option* [*options*] [*priority*] *PID*

**modbox chrt** **--pid** *PID*

# DESCRIPTION

**modbox chrt** shows or changes the real-time scheduling attributes of a
running process, or runs a command with the requested attributes.

Two operating shapes are supported:

*Set and run* — give a *priority* (for policies that require one) followed by
a *command* and its arguments. The command is executed with the selected
scheduling policy, and the exit status of **modbox chrt** is the exit status
of that command.

*Set or query an existing process* — with **--pid**, apply a policy to an
existing *PID*, or, when no policy option is supplied, report the current
policy and priority of that *PID*.

With no policy option, the launch form defaults to **SCHED_RR**. Because
**SCHED_RR** requires an explicit priority, a bare `chrt <command>` is a
misuse; select a policy that does not need one (for example **-o**) or give a
priority.

# POLICIES

`-o`, `--other`
:   Set scheduling policy to **SCHED_OTHER** (the default time-sharing policy).

`-b`, `--batch`
:   Set scheduling policy to **SCHED_BATCH**.

`-i`, `--idle`
:   Set scheduling policy to **SCHED_IDLE**.

`-f`, `--fifo`
:   Set scheduling policy to **SCHED_FIFO** (real-time, requires a *priority*).

`-r`, `--rr`
:   Set scheduling policy to **SCHED_RR** (real-time, requires a *priority*;
    this is the default for the launch form).

`-d`, `--deadline`
:   Set scheduling policy to **SCHED_DEADLINE**. See **NOTES** for the
    limited support provided.

`-e`, `--ext`
:   Set scheduling policy to **SCHED_EXT** where the kernel provides it.

# OPTIONS

`-p`, `--pid`
:   Operate on an existing process given by *PID* rather than launching a
    command. With a policy option, two numeric arguments are accepted as
    *priority* then *PID*; when the selected policy does not require a
    priority, a single numeric argument is taken as the *PID*. With no policy
    option, the *PID* is queried.

`-a`, `--all-tasks`
:   Operate on all the tasks (threads) for a given *PID*.

`-m`, `--max`
:   Show the minimum and maximum valid priorities for each policy, then exit.

`-R`, `--reset-on-fork`
:   Set the reset-on-fork flag, so that a child forked by the process returns
    to the default scheduling policy.

`-T`, `--sched-runtime` *ns*
`-P`, `--sched-period` *ns*
`-D`, `--sched-deadline` *ns*
:   Scheduling parameters for **SCHED_DEADLINE**. These options are accepted
    for compatibility with util-linux **chrt** but are not applied; see
    **NOTES**.

`-v`, `--verbose`
:   When querying, also print the valid priority range for the reported policy.

`-h`, `--help`
:   Display help text and exit.

`-V`, `--version`
:   Print version information and exit.

# EXIT STATUS

`0` on success. `1` when the request could not be carried out (no such
process, permission denied, a policy that requires a priority was given
without one) or when no command or priority was specified. `2` when the
command line could not be parsed — an unrecognized option or an unexpected
argument. `127` when a command named by a numeric argument could not be
executed. When a command is launched, its exit status is propagated
unchanged.

# NOTES

Because the real-time policies **SCHED_FIFO** and **SCHED_RR** and the
**SCHED_DEADLINE** policy are privileged, applying them normally requires
**CAP_SYS_NICE**; an unprivileged attempt fails with *Operation not
permitted*. **SCHED_BATCH**, **SCHED_IDLE**, and **SCHED_OTHER** can usually
be applied by ordinary users.

The following deliberate divergences from util-linux **chrt** apply:

* **SCHED_DEADLINE** is accepted as a policy name and the **-T**/**-P**/**-D**
  parameters are parsed, but no deadline parameters are applied.
* Querying a process prints its policy and priority but not the extra
  *current runtime parameter* line that util-linux prints for
  **SCHED_DEADLINE**.
* Usage errors exit with status **2** rather than util-linux's **1**.

# EXAMPLES

Show the valid priority ranges for every policy:

    modbox chrt -m

Query the scheduling policy of process 1234:

    modbox chrt -p 1234

Run a command with the default time-sharing policy:

    modbox chrt -o 0 make -j8

Run a command at real-time priority 10 under **SCHED_FIFO**:

    modbox chrt -f 10 ./audio-render

Set the policy of process 1234 to **SCHED_BATCH** without a priority:

    modbox chrt -p -b 1234

Set process 1234 to **SCHED_RR** priority 20, resetting on fork:

    modbox chrt -p -R -r 20 1234

# SEE ALSO

**modbox-nice**(1), **modbox-renice**(1), **modbox**(1)
