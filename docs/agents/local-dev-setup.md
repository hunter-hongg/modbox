# Local dev environment (read-only `/home` host)

This host mounts `/home` **read-only** except for individual bind-mounted
directories, so the system Homebrew prefix (`/home/linuxbrew/.linuxbrew`) and the
default `HOMEBREW_CACHE` (`~/.cache/Homebrew`) are not writable. `brew install`
against the system prefix therefore fails with a permissions error.

The working copy at `/home/hunter/projects/modbox` *is* writable, so the
toolchain is installed into a self-contained Homebrew prefix inside the repo:
`.brew/` (git-ignored).

## What is already provided

`make`, `python3`, `pkg-config`, and a Fedora `gcc` runtime are on the system.
`openssl` also resolves to `/usr/lib64/pkgconfig/openssl.pc`, and `bzip2` /
`libselinux` resolve through the read-only linuxbrew prefix. Those four need no
install.

## Installed into `.brew/`

| Package | Why |
| --- | --- |
| `gcc` | supplies `g++` (the system `gcc-c++` and `/usr/include/c++` are absent) |
| `ftxui` | build dependency (`PKGS` in the Makefile) |
| `acl` | provides `libacl.pc` |
| `xz` | provides `liblzma.pc` |
| `zstd` | provides `libzstd.pc` |
| `minizip` | build dependency |
| `pandoc` | required by `make man` |
| `perl` | needed only while building a source-only formula |

## Using the toolchain

`PKG_CONFIG_PATH` must contain **both** prefixes: the repo one for the newly
installed libraries and the system linuxbrew one for `bzip2` / `libselinux`.

```bash
cd /home/hunter/projects/modbox
B="$PWD/.brew/Homebrew"
export PATH="$B/opt/gcc/bin:$B/opt/pandoc/bin:$PATH"
export PKG_CONFIG="$B/lib/pkgconfig:/home/linuxbrew/.linuxbrew/lib/pkgconfig"

make compile LINUXBREW_PKGCONFIG="$PKG_CONFIG" CXX="$B/opt/gcc/bin/g++-16"
make man LINUXBREW_PKGCONFIG="$PKG_CONFIG"
make test
```

`CXX=` is required because the Makefile defaults to `g++`, which does not exist
on this host. `LINUXBREW_PKGCONFIG=` is overridden for the same reason: the
Makefile hard-codes the read-only prefix at line 55.

`make man` must run **before** `make test`, and `pandoc` must be on `PATH` (it
is in `$B/opt/pandoc/bin`, which the `PATH` above covers). The man-page tests
assert that `make man` has actually rendered `build/man/modbox-<cmd>.1`, and
`make clean` deletes `build/man`. Without pandoc, `make man` prints "pandoc
not found" and exits 1, so those 4 assertions in `tests/test_man_issue62.sh`
fail on an empty `build/man` even though nothing is wrong with the command.

## Reinstalling into `.brew/`

The prefix was created by cloning Homebrew and marking its `bin/brew`
non-writable. That `chmod a-w` is deliberate: Homebrew refuses to self-install
when `bin/brew` is writable, because the install sandbox would then be able to
replace the running `brew` binary.

```bash
B="$PWD/.brew/Homebrew"
export HOMEBREW_CACHE="$PWD/.brew/cache"     # else brew writes to read-only ~/.cache
export HOMEBREW_LOGS="$PWD/.brew/logs"       # ditto for Logs
export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1
git clone --depth 1 https://github.com/Homebrew/brew "$B"
chmod a-w "$B/bin/brew"
"$B/bin/brew" install gcc ftxui acl xz zstd minizip pandoc
```

## Known-unfixable-without-root issues

* `HOMEBREW_PREFIX` is not `/home/linuxbrew/.linuxbrew`, a Homebrew "Tier 3"
  configuration, so formulae without a Linux bottle build from source.
* `openssl@3` is not installed: it has no Linux bottle and its `Configure`
  hard-codes `/usr/bin/perl`, whose module path is read-only and missing
  `lib.pm`. Homebrew strips `PERL` / `PERL5LIB` from the build environment, so
  the build cannot be redirected to the Homebrew Perl. The system
  `openssl.pc` already satisfies the build.

## Expected test failures (environment, not regressions)

`make test` reports `3623 passed, 5 failed`. All five are container artefacts:

* `test_restorecon.sh` (2) — SELinux is `Disabled` on this host.
* `test_pgrep.sh` (1) — expects `2 kthreadd`; the PID namespace has a different
  PID 2.
* `test_users.sh` (1) — no login sessions (`who` is empty).
* `test_fuser.sh` (1) — `fuser -m /` finds no mount users.

## `make lint` / clang-tidy

`clang-tidy` is **not** installed. It ships in the `llvm` formula, which has no
Linux bottle and takes well over 10 minutes to build from source here. The
Makefile already degrades gracefully, printing `clang-tidy not found; install it
to enable linting` and exiting 0.
