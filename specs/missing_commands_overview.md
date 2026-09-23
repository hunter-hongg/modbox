# ModBox: GNU CoreUtils Compatibility — Missing Commands Overview

## Status: complete (2026-09-15)

ModBox implements **200 registered commands**, including **all standard GNU
coreutils commands** plus additional utilities. There are currently **no
known missing coreutils commands**.

The two historically-listed gaps — `pinky` and `stdbuf` — are now implemented
(`src/commands/pinky.cpp`, `src/commands/stdbuf.cpp`), each with a man page and
a test.

## Verification

The canonical command set is `registered_cmds.txt`, regenerated from the
`REGISTER_COMMAND` macros in `src/commands/`. The registry-driven coverage
check in `tests/test_man_pages.sh` asserts that **every** registered command
has both a man page source (`docs/man/modbox-<cmd>.1.md`) and a `MAN_SOURCES`
entry in the `Makefile`.

To re-verify manually:

```bash
# List every registered command (the binary is the source of truth)
./target/modbox help | sed -n 's/^ \([a-zA-Z0-9[]*\) .*/\1/p' | sort -u

# Check man-page coverage for each registered command
while IFS= read -r cmd; do
    [[ -z "$cmd" || "$cmd" == \#* ]] && continue
    [[ -f "docs/man/modbox-${cmd}.1.md" ]] || echo "missing man: $cmd"
    grep -q "modbox-${cmd}.1.md" Makefile || echo "missing Makefile entry: $cmd"
done < registered_cmds.txt
```

## Aliases and non-command names

Some registered names are aliases and are intentionally absent from
`registered_cmds.txt` (they are documented by the page of the command they
alias):

- `[` — alias for `test`; documented by `modbox-test(1)`.
- `netcat` — alias for `nc` (`docs/man/modbox-netcat.1.md`).
- `dir`, `vdir` — variants of `ls` (`docs/man/modbox-dir.1.md`,
  `docs/man/modbox-vdir.1.md`).

## Notes on completeness

- All GNU coreutils commands are present.
- Additional non-coreutils utilities are included (e.g. `awk`, `jq`, `sed`,
  `curl`, `wget`, `find`, `grep`, `perf`, `tcpdump`, `tar`, `zip`/`unzip`,
  `xz`/`zstd`/`gzip`, SELinux tools).
- `sh` intentionally has **no** dedicated test (`tests/test_sh.sh` was removed
  on purpose; see `AGENTS.md`).
