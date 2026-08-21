% MODBOX-DIFF3(1) modbox | User Commands
% modbox project
% 2026-08-21

# NAME

modbox-diff3 - compare three files line by line

# SYNOPSIS

**modbox diff3** [*OPTION*]... *MYFILE* *BASEFILE* *YOURFILE*

# DESCRIPTION

Compare three files line by line and report how *MYFILE* and *YOURFILE* differ
from *BASEFILE*. This is the classic three-way merge comparison: it identifies
conflicts where both sides changed the same region of the base file.

The three arguments are semantically ordered:

**MYFILE**
:   Your version of the file (left side of the merge).

**BASEFILE**
:   The common ancestor (the original before divergence).

**YOURFILE**
:   Their version of the file (right side of the merge).

# OPTIONS

## Output format options

**-A**, **--show-all**
:   Output all changes from both sides, with conflict markers for
    overlapping changes. This is equivalent to **-m** but also shows
    non-overlapping changes alongside the conflict markers.

**-e**, **--ed**
:   Output an ed script that, when applied to *BASEFILE*, produces a file
    equivalent to merging *MYFILE* and *YOURFILE*. No conflict markers are
    printed; overlapping changes are silently discarded.

**-E**, **--show-overlap**
:   Like **-e** but print conflict markers (in ed syntax) for overlapping
    changes where both sides modified the same region.

**-3**, **--easy-only**
:   Output only non-overlapping changes. Overlapping regions are suppressed.
    This is the least noisy output mode.

**-x**, **--overlap-only**
:   Output only overlapping changes (conflicts). Non-conflicting regions
    are suppressed.

**-X**
:   Like **-x** but include conflict markers for each overlapping change.

**-m**, **--merge**
:   Output a merged file with conflict markers. This is the default output
    mode and produces a file suitable for manual conflict resolution.

## General options

**-a**, **--text**
:   Treat all files as text, even if they contain NUL bytes.

**-T**, **--initial-tab**
:   Add a tab character before each line in the output to make column
    alignment clearer.

**-L**, **--label=LABEL**
:   Use *LABEL* instead of the filename in the output. Can be specified
    up to three times to label MYFILE, BASEFILE, and YOURFILE
    respectively.

**-h**, **--help**
:   Display help and exit.

# OUTPUT FORMATS

## Default merge output (-m)

Produces a file with standard conflict markers:

```
<<<<<<< label1
your changes
=======
their changes
>>>>>>> label2
```

Lines without conflict markers are unchanged from the base file.

## Ed script output (-e)

Produces valid ed(1) commands. For non-conflicting regions, the script
applies changes directly. Overlapping changes are silently skipped.

## Show-all output (-A)

Combines all changes with conflict markers, showing both overlapping and
non-overlapping regions in a single pass.

# CONFLICT TYPES

diff3 classifies changes into three categories:

**Easy (non-overlapping)**
:   Only one side changed a region. No conflict; the change is applied
    directly.

**Overlap (conflicting)**
:   Both sides changed the same region of the base file. Marked with
    conflict markers for manual resolution.

**Deleted overlap**
:   One side deleted a region that the other side modified. Treated as a
    conflict.

# EXIT STATUS

`0`
:   All three files are identical, or the merge was successful with no
    conflicts (in **--easy-only** or **--ed** mode).

`1`
:   Differences were found (including conflicts).

`2`
:   An error occurred (e.g., file not found, I/O error).

# NOTES

- diff3 uses an LCS-based diff engine internally, shared with the modbox
  **diff** command.
- The **MYFILE**, **BASEFILE**, **YOURFILE** argument order is significant
  and differs from some older diff3 implementations.
- Line length is capped at 1 MiB per line.

## Differences from GNU diff3

Not implemented: **-i** NUM (skip every N-th output line),
**--old-new-match** (treat newly added lines as matching), `--strip-trailing-cr`
is accepted as a flag but has no visible effect in most cases.

# EXAMPLES

```bash
# Basic three-way comparison
modbox diff3 mine.txt base.txt yours.txt

# Merge with conflict markers (default)
modbox diff3 -m original.txt modified_a.txt modified_b.txt

# Output ed script for automated patching
modbox diff3 -e base.txt ours.txt theirs.txt > merge.ed

# Show only conflicts
modbox diff3 -x base.txt ours.txt theirs.txt

# Label the output
modbox diff3 -L mine -L base -L yours base.txt mine.txt yours.txt

# Suppress overlapping changes
modbox diff3 -3 base.txt ours.txt theirs.txt
```

# SEE ALSO

**modbox-diff**(1), **modbox-merge**(1), **modbox**(1)
