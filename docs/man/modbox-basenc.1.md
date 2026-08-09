% MODBOX-BASENC(1) modbox | User Commands
% modbox project
% 2026-08-09

# NAME

modbox-basenc - encode or decode data using various byte encodings

# SYNOPSIS

**modbox basenc** *ENCODING* [*OPTION*]... [*FILE*]

# DESCRIPTION

Encode or decode FILE, or standard input, to standard output, using the
encoding selected by the required *ENCODING* argument. Exactly one encoding
must be given.

With no FILE, or when FILE is `-`, read from standard input.

The encodings follow the byte-oriented schemes in RFC 4648, plus two
bit-order variants of base2.

# OPTIONS

One of the following encoding selectors is required:

**--base64**
:   Base64 (RFC 4648 section 4).

**--base64url**
:   Base64url (RFC 4648 section 5) — URL- and filename-safe variant.

**--base32**
:   Base32 (RFC 4648 section 6).

**--base32hex**
:   Base32hex (RFC 4648 section 7) — extended hex alphabet.

**--base16**
:   Base16 / hexadecimal (RFC 4648 section 8).

**--base2msbf**
:   Base2, most significant bit first.

**--base2lsbf**
:   Base2, least significant bit first.

**-d**, **--decode**
:   Decode data. Without this, data are encoded.

**-i**, **--ignore-garbage**
:   When decoding, ignore non-alphabet characters (other than padding and
    whitespace) instead of reporting an error.

**-w**, **--wrap**=*COLS*
:   Wrap encoded lines after *COLS* characters (default 76). Use `0` to
    disable line wrapping.

**-h**, **--help**
:   Display help and exit.

# EXAMPLES

```bash
# Encode with base64url (URL-safe)
modbox basenc --base64url file.bin > file.b64url

# Encode with base16 (hex)
modbox basenc --base16 file.bin

# Decode a base32 stream
modbox basenc --base32 -d file.b32 > file.bin

# Encode standard input, no line wrapping
echo "hello" | modbox basenc --base64 -w 0

# Decode, tolerating stray characters in the input
modbox basenc --base32 -d -i file.b32
```

# EXIT STATUS

`0`
:   Encoding or decoding completed successfully.

non-zero
:   At least one input could not be read, no encoding was selected, more
    than one encoding was selected, or decoding encountered a non-alphabet
    byte (without **--ignore-garbage**).

# NOTES

- The *ENCODING* argument is mandatory and only one may be supplied.
- Encoded output is wrapped at 76 columns by default; disable wrapping
  with **-w 0**.
- This page documents the current implementation; where it differs from
  GNU coreutils, the modbox behavior is authoritative.

# SEE ALSO

**modbox-base64**(1), **modbox-base32**(1), **modbox-cat**(1),
**modbox**(1)
