% MODBOX-FMT(1) modbox | User Commands
% modbox project
% 2026-08-02

# NAME

modbox-fmt - reformat paragraph text

# SYNOPSIS

**modbox fmt** [*OPTION*]... [*FILE*]...

# DESCRIPTION

Reformat each paragraph to fit within a specified width. Paragraphs are separated by blank lines. With no FILE, read from standard input.

# OPTIONS

**-w**, **--width**=*N*
:   Maximum output width, default 72.

**-u**, **--uniform-spacing**
:   One space between words, two between sentences.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
modbox fmt file.txt
modbox fmt -w 80 < input
modbox fmt -w 100 -u paragraph.txt
cat README.md | modbox fmt -w 72
```

# NOTES

- Input paragraphs are reformatted but blank lines are preserved.

# EXIT STATUS

`0` on success, non-zero on error.

# SEE ALSO

**modbox-fold**(1), **modbox**(1)
