% MODBOX-BASE64(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-base64 - base64 encode or decode data

# SYNOPSIS

**modbox base64** [*OPTION*]... [*FILE*]

# DESCRIPTION

Base64 encode or decode FILE, or standard input, to standard output.

With no FILE, or when FILE is `-`, read from standard input.

The data are encoded as described for the base64 alphabet in RFC 4648.
When decoding, the input may contain newlines in addition to the bytes of
the formal base64 alphabet. Use **--ignore-garbage** to attempt to recover
from any other non-alphabet bytes in the encoded stream.

# OPTIONS

**-d**, **--decode**
:   Decode data. Without this, data are encoded.

**-i**, **--ignore-garbage**
:   When decoding, ignore non-alphabet characters (other than the padding
    `=` and whitespace) instead of reporting an error.

**-w**, **--wrap**=*COLS*
:   Wrap encoded lines after *COLS* characters (default 76). Use `0` to
    disable line wrapping.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Encode a file (wrapped at 76 columns by default)
modbox base64 file.bin > file.b64

# Encode without line wrapping
modbox base64 -w 0 file.bin

# Decode a file
modbox base64 -d file.b64 > file.bin

# Encode standard input from a pipeline
echo "hello" | modbox base64

# Decode, tolerating stray characters in the input
modbox base64 -d -i file.b64
```

# EXIT STATUS

`0`
:   Encoding or decoding completed successfully.

non-zero
:   At least one input could not be read, or decoding encountered a
    non-alphabet byte (without **--ignore-garbage**), or input could not
    be decoded.

# NOTES

- Encoded output is wrapped at 76 columns by default; disable wrapping
  with **-w 0**.
- Decoding accepts the standard base64 alphabet and treats `=` as padding.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-base32**(1), **modbox-basenc**(1), **modbox-cat**(1),
**modbox**(1)
