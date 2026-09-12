% MODBOX-SDIFF(1) modbox | User Commands
% modbox project
% 2026-09-12

# NAME

modbox-sdiff - side-by-side merge of file differences

# SYNOPSIS

**modbox sdiff** [*OPTION*]... FILE1 FILE2

**modbox sdiff** [*OPTION*]... **-o** *OUTPUT* FILE1 FILE2

# DESCRIPTION

Display FILE1 and FILE2 side by side, with identical lines joined by a
column of spaces and differing lines marked by a gutter symbol that
indicates the kind of change:

- `|` - the two lines differ (a replacement)
- `<` - the line appears only in FILE1
- `>` - the line appears only in FILE2
- `(` - the line appears only in FILE1 and is context (`-l`)

Lines that are equal are shown once on each side.  When a line is
present on only one side, the other side is left blank but is padded so
the two columns stay aligned.

With **-o** *OUTPUT*, `sdiff` additionally acts as an interactive merge
tool: for each run of differing lines it prompts for a command and
writes the chosen lines to *OUTPUT*.  Unchanged lines are copied through
verbatim, so *OUTPUT* always receives a complete merged file.  `sdiff`
then exits without printing the side-by-side view to standard output.

# OPTIONS

## Comparison options

**-i**, **--ignore-case**
:   Consider uppercase and lowercase letters equivalent.

**-E**, **--ignore-tab-expansion**
:   Treat sequences that differ only by tab expansion as equal.

**-Z**, **--ignore-trailing-space**
:   Ignore whitespace at the end of a line.

**-b**, **--ignore-space-change**
:   Ignore changes in the amount of whitespace, and ignore trailing
    whitespace.

**-W**, **--ignore-all-space**
:   Ignore all whitespace within a line.

**-B**, **--ignore-blank-lines**
:   Ignore changes whose lines are all blank.

**-I** *RE*, **--ignore-matching-lines**=*RE*
:   Ignore changes whose lines all match *RE*.  *RE* may be a plain
    substring, an anchored `^`/`$` form, or contain the single-character
    wildcard `.` and classes such as `[abc]`, `[^abc]` and `[a-z]`.

**--strip-trailing-cr**
:   Strip a trailing carriage return at the end of each input line, so
    CRLF files compare equal to their LF counterparts.

**-a**, **--text**
:   Treat the inputs as text even if they appear to be binary.

## Output layout

**-l**, **--left-column**
:   Show only the left column for lines that are identical.

**-s**, **--suppress-common-lines**
:   Do not show lines that are identical.

**-t**, **--expand-tabs**
:   Expand tabs to spaces in the output.

**-w** *NUM*, **--width**=*NUM*
:   Set the output width to *NUM* columns (default 130).  The width is
    split into two halves separated by a gutter; see NOTES for the exact
    geometry.

## Interactive merge

**-o** *FILE*, **--output**=*FILE*
:   Operate interactively, merging the differences into *FILE*.  `sdiff`
    prompts on standard output for a command at each set of differing
    lines.  The recognized commands are:

    - `l` or `1` - use the left (FILE1) lines
    - `r` or `2` - use the right (FILE2) lines
    - `s` - silently use the common lines (or skip the difference)
    - `v` - use the left lines and open them in an editor
    - `e` - use an empty line insertion point and edit it
    - `eb`, `ed`, `el`/`e1`, `er`/`e2` - edit both, the left, or the
      right lines with the editor, optionally keeping the header
    - `q` - quit without writing further output (exit status 2)

    The editor is taken from the `EDITOR` environment variable and
    defaults to `vi`.  End-of-file on standard input behaves like `q`.

## Miscellaneous

**-h**, **--help**
:   Display help and exit.

**-V**, **--version**
:   Output version information and exit.

# OPTION PRECEDENCE

When several whitespace options are combined, **--ignore-all-space**
implies the others.  **-l** and **-s** may be combined; identical lines
are then shown neither in full nor on the right.

# EXAMPLES

```bash
# Show two files side by side
modbox sdiff old.txt new.txt

# Ignore case and all whitespace
modbox sdiff -i -W old.txt new.txt

# Suppress the common lines, keeping only the changes
modbox sdiff -s old.txt new.txt

# Use a narrower, tabbed layout
modbox sdiff -w 80 old.txt new.txt

# Merge interactively into a new file
modbox sdiff -o merged.txt old.txt new.txt

# Restrict the width and use the left column for equal lines
modbox sdiff -l -w 60 a.c b.c
```

# EXIT STATUS

`0`
:   The inputs are identical (or all differences were ignored by the
    requested comparison options).

`1`
:   The inputs differ.

`2`
:   An error occurred (e.g., a file could not be opened, an option was
    invalid, the operand count was wrong, or **-o** was not given a
    writable output file).

# NOTES

- When invoked without **-o**, `sdiff` behaves exactly like
  `diff -y` and produces byte-identical output.
- The output is split into a left column and a right column separated by
  three gutter columns.  Half the width (rounded as GNU does) is
  assigned to each side; the remainder becomes the gutter.  Tabs in the
  input are padded with tab characters unless **-t** is given.
- `sdiff` orders its changes using a line-oriented comparison.  For
  files that contain many repeated lines, the placement of an individual
  change within a run of equivalent lines may differ from GNU
  diffutils, although both remain valid minimal edit scripts.
- For a single-column diff, use **modbox-diff**(1); for a three-way
  merge, use **modbox-diff3**(1).

# SEE ALSO

**modbox-diff**(1), **modbox-diff3**(1), **modbox-cmp**(1),
**modbox-comm**(1), **modbox**(1)
