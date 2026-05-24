# cp Options Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 10+ missing GNU coreutils cp options to modbox's `cp` command, implemented in priority order with TDD.

**Architecture:** All changes are confined to `src/commands/cp.c`. Tests go in `tests/run_tests.sh`. Each option is a new `arg_lit`/`arg_str` entry in the argtable, with corresponding logic in `copy_file`, `copy_recursive`, or `cp_command`. Options are organized into phases by priority. A shared options struct consolidates all boolean flags to avoid parameter explosion.

**Tech Stack:** C99, GLib, argtable3, POSIX syscalls (stat, link, symlink, utimensat, chmod, chown, remove, unlink, rename)

**Plan location:** `docs/superpowers/plans/2026-05-21-cp-options.md`

---

## File Structure

All changes are to existing files — no new files needed:

| File | Responsibility | Change type |
|------|---------------|-------------|
| `src/commands/cp.c` | Entire cp implementation | Modify |
| `tests/run_tests.sh` | All test cases | Modify |
| `include/commands/cp.h` | Function signature (unchanged) | None |

Implementation note: `copy_file()` and `copy_recursive()` currently take `(src, dst, is_verbose)`. To avoid parameter explosion, each task that adds a flag will convert these to take a single `struct cp_options *opts` pointer holding all boolean flags.

---

### Phase 1: Overwrite Control (-i, -f, -n, -R)

### Task 1: Refactor to shared options struct

**Files:**
- Modify: `src/commands/cp.c`

- [ ] **Step 1: Add options struct and convert copy_file / copy_recursive signatures**

Add this near the top of `cp.c` after the `#define`s:

```c
/* Shared options passed through copy chain */
struct cp_options {
    int is_recursive;
    int is_verbose;
    int is_interactive;
    int is_force;
    int is_no_clobber;
};
```

Change `copy_file` signature from:
```c
static int copy_file(const char *src, const char *dst, int is_verbose)
```
to:
```c
static int copy_file(const char *src, const char *dst, const struct cp_options *opts)
```

Inside `copy_file`, replace `(is_verbose)` with `(opts->is_verbose)`.

Similarly change `copy_recursive` from `(src, dst, is_verbose)` to `(src, dst, opts)`.

Update all calls: `copy_file(src, dst, is_verbose)` → `copy_file(src, dest_path, opts)` and `copy_recursive(src, dest_path, is_verbose)` → `copy_recursive(src, dest_path, opts)`.

In `cp_command`, replace local `int is_recursive` / `int is_verbose` with:
```c
struct cp_options opts = {
    .is_recursive = (recursive_opt->count > 0),
    .is_verbose = (verbose_opt->count > 0),
    .is_interactive = 0,
    .is_force = 0,
    .is_no_clobber = 0,
};
```

Then pass `&opts` wherever `copy_file` or `copy_recursive` is called.

- [ ] **Step 2: Build and run existing tests to verify refactor didn't break anything**

```bash
make && bash tests/run_tests.sh
```

Expected: All cp tests pass (green), no compilation warnings.

- [ ] **Step 3: Commit**

```bash
git add src/commands/cp.c
git commit -m "refactor(cp): introduce cp_options struct for flag passing"
```

---

### Task 2: Add -n / --no-clobber (do not overwrite existing files)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

Add to test section around line 310 (after "error: missing destination" test) in `tests/run_tests.sh`:

```bash
echo "  ── -n : no-clobber ──"
echo "first content" > "$TMPDIR"/cp_n_src.txt
echo "second content" > "$TMPDIR"/cp_n_dst.txt
"$MODBOX" cp -n "$TMPDIR"/cp_n_src.txt "$TMPDIR"/cp_n_dst.txt 2>/dev/null || true
# With -n, dst should NOT be overwritten
assert_cmd "second content" cat "$TMPDIR"/cp_n_dst.txt
```

Run:
```bash
bash tests/run_tests.sh
```
Expected: FAIL — "cp -n" will either error on unrecognized flag or overwrite the file.

- [ ] **Step 2: Add arg_lit for -n in cp_command**

In `cp_command`, after the `verbose_opt` definition:
```c
struct arg_lit *no_clobber_opt =
    arg_lit0("n", "no-clobber", "do not overwrite an existing file");
```

Add to `argtable[]` array and connect to options struct:
```c
struct cp_options opts = {
    .is_recursive = (recursive_opt->count > 0),
    .is_verbose = (verbose_opt->count > 0),
    .is_interactive = 0,
    .is_force = 0,
    .is_no_clobber = (no_clobber_opt->count > 0),
};
```

Update the `argtable[]` array:
```c
void *argtable[] = {recursive_opt, verbose_opt, no_clobber_opt, files_arg, end};
```

- [ ] **Step 3: Add no-clobber check in copy_file**

At the very top of `copy_file`, before opening any files:
```c
if (opts->is_no_clobber) {
    struct stat st;
    if (stat(dst, &st) == 0) {
        if (opts->is_verbose) {
            (void)printf("'%s' -> '%s' (skipped: already exists)\n", src, dst);
        }
        return 0; /* not an error, just skip */
    }
}
```

- [ ] **Step 4: Run test to verify passes**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS on the no-clobber test, all previous tests still pass.

- [ ] **Step 5: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -n / --no-clobber option"
```

---

### Task 3: Add -f / --force (remove existing destination before copy)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

In the test section, after the -n test:
```bash
echo "  ── -f : force overwrite read-only destination ──"
echo "source content" > "$TMPDIR"/cp_f_src.txt
echo "read-only dest" > "$TMPDIR"/cp_f_dst.txt
chmod 444 "$TMPDIR"/cp_f_dst.txt
"$MODBOX" cp -f "$TMPDIR"/cp_f_src.txt "$TMPDIR"/cp_f_dst.txt 2>/dev/null || true
assert_cmd "source content" cat "$TMPDIR"/cp_f_dst.txt
```

Run:
```bash
bash tests/run_tests.sh
```
Expected: FAIL — `fopen(dst, "wb")` will fail on read-only file.

- [ ] **Step 2: Add arg_lit for -f**

In `cp_command`, after `no_clobber_opt`:
```c
struct arg_lit *force_opt =
    arg_lit0("f", "force", "remove existing destination file before copying");
```

Add to argtable and update opts:
```c
.is_force = (force_opt->count > 0),
```

- [ ] **Step 3: Add force logic in copy_file**

In `copy_file`, before `fopen(fdst, "wb")`, add:
```c
if (opts->is_force) {
    /* Try to remove existing destination file */
    struct stat dst_st;
    if (stat(dst, &dst_st) == 0) {
        if (remove(dst) != 0) {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr, "cp: %s: Cannot remove destination\n", dst);
            // NOLINTNEXTLINE(bugprone-unused-return-value)
            (void)fclose(fsrc);
            return -1;
        }
    }
}
```

- [ ] **Step 4: Run test to verify passes**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS on the -f test.

- [ ] **Step 5: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -f / --force option"
```

---

### Task 4: Add -i / --interactive (prompt before overwrite)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

```bash
echo "  ── -i : interactive (auto-yes via pipe) ──"
echo "old content" > "$TMPDIR"/cp_i_src.txt
echo "new content" > "$TMPDIR"/cp_i_dst.txt
echo "y" | "$MODBOX" cp -i "$TMPDIR"/cp_i_src.txt "$TMPDIR"/cp_i_dst.txt 2>/dev/null || true
assert_cmd "new content" cat "$TMPDIR"/cp_i_dst.txt

echo "  ── -i : interactive decline ──"
echo "keep content" > "$TMPDIR"/cp_i_src2.txt
echo "original content" > "$TMPDIR"/cp_i_dst2.txt
echo "n" | "$MODBOX" cp -i "$TMPDIR"/cp_i_src2.txt "$TMPDIR"/cp_i_dst2.txt 2>/dev/null || true
assert_cmd "original content" cat "$TMPDIR"/cp_i_dst2.txt
```

Expected: FAIL — no -i flag exists yet.

- [ ] **Step 2: Add arg_lit for -i**

```c
struct arg_lit *interactive_opt =
    arg_lit0("i", "interactive", "prompt before overwrite");
```

Wire into opts:
```c
.is_interactive = (interactive_opt->count > 0),
```

- [ ] **Step 3: Add interactive prompt logic in copy_file**

At the top of `copy_file`, after the no-clobber check:
```c
if (opts->is_interactive) {
    struct stat st;
    if (stat(dst, &st) == 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: overwrite '%s'? ", dst);
        int ch = fgetc(stdin);
        /* consume rest of line */
        if (ch != EOF && ch != '\n') {
            int c;
            do { c = fgetc(stdin); } while (c != EOF && c != '\n');
        }
        if (ch != 'y' && ch != 'Y') {
            if (opts->is_verbose) {
                (void)printf("'%s' -> '%s' (skipped: declined)\n", src, dst);
            }
            return 0;
        }
    }
}
```

Note: `-f` and `-i` interaction per POSIX: if both `-f` and `-i` are given, `-f` takes precedence. So add a guard:
```c
if (opts->is_interactive && !opts->is_force) {
    ...prompt...
}
```

- [ ] **Step 4: Run test to verify passes**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -i / --interactive option"
```

---

### Task 5: Add -R as alias for -r (recursive)

**Files:**
- Modify: `src/commands/cp.c`

- [ ] **Step 1: Add -R arg_lit**

In `cp_command`, change the `recursive_opt` line or add a second one:
```c
struct arg_lit *recursive_opt =
    arg_lit0("r", "recursive", "copy directories recursively (same as -R)");
struct arg_lit *recursive_cap_opt =
    arg_lit0("R", NULL, "copy directories recursively (same as -r)");
```

Update opts to OR them together:
```c
.is_recursive = (recursive_opt->count > 0) || (recursive_cap_opt->count > 0),
```

Add both to the argtable array.

- [ ] **Step 2: Build and test**

```bash
make
# Quick manual check - but existing tests already cover -r behavior
bash tests/run_tests.sh
```

Expected: Compilation clean, all tests pass. The -R flag is just an alias; existing -r tests remain passing.

- [ ] **Step 3: Add test for -R**

In the test section after the -r test:
```bash
echo "  ── -R : recursive (alias for -r) ──"
"$MODBOX" cp -R "$TMPDIR"/cp_src_dir "$TMPDIR"/cp_cap_r_dst 2>/dev/null || true
assert_cmd "nested" cat "$TMPDIR"/cp_cap_r_dst/sub/file.txt
```

Run tests:
```bash
bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -R as alias for -r"
```

---

### Phase 2: Copy Mode Selection (-l, -s, -p)

### Task 6: Add -l / --link (hard link instead of copy)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Add `is_link` to the options struct**

```c
int is_link;
```

- [ ] **Step 2: Write failing test**

```bash
echo "  ── -l : hard link instead of copy ──"
echo "link content" > "$TMPDIR"/cp_l_src.txt
"$MODBOX" cp -l "$TMPDIR"/cp_l_src.txt "$TMPDIR"/cp_l_dst.txt 2>/dev/null || true
# Same inode = hard link
src_inode=$(stat -c '%i' "$TMPDIR"/cp_l_src.txt)
dst_inode=$(stat -c '%i' "$TMPDIR"/cp_l_dst.txt)
if [[ "$src_inode" == "$dst_inode" ]]; then
    pass "cp -l → hard link (same inode: $src_inode)"
else
    fail "cp -l — inodes differ ($src_inode vs $dst_inode)"
fi
assert_cmd "link content" cat "$TMPDIR"/cp_l_dst.txt
```

Expected: FAIL — -l not recognized.

- [ ] **Step 3: Add arg_lit for -l**

```c
struct arg_lit *link_opt =
    arg_lit0("l", "link", "hard link files instead of copying");
```

Wire up and add to argtable.

- [ ] **Step 4: Add link logic in copy_file and copy_recursive**

In `copy_file`, right at the top (before anything else), add:
```c
if (opts->is_link) {
    if (link(src, dst) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: cannot create hard link\n", dst);
        return -1;
    }
    if (opts->is_verbose) {
        (void)printf("'%s' -> '%s'\n", src, dst);
    }
    return 0;
}
```

In `copy_recursive`, for directories when `is_link` is set, fall through to normal recursive copy (can't hard-link directories anyway — EPERM). Just skip linking for directories. Add at the top of `copy_recursive`, before the regular file check:
```c
if (opts->is_link && S_ISREG(src_stat.st_mode)) {
    return copy_file(src, dst, opts);
}
```

- [ ] **Step 5: Run tests**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -l / --link option"
```

---

### Task 7: Add -s / --symbolic-link (symlink instead of copy)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Add `is_symlink` to options struct**

```c
int is_symlink;
```

- [ ] **Step 2: Write failing test**

```bash
echo "  ── -s : symbolic link instead of copy ──"
echo "sym content" > "$TMPDIR"/cp_s_src.txt
"$MODBOX" cp -s "$TMPDIR"/cp_s_src.txt "$TMPDIR"/cp_s_dst.txt 2>/dev/null || true
if [[ -L "$TMPDIR"/cp_s_dst.txt ]]; then
    pass "cp -s → symlink created"
else
    fail "cp -s — not a symlink"
fi
assert_cmd "sym content" cat "$TMPDIR"/cp_s_dst.txt
```

Expected: FAIL.

- [ ] **Step 3: Add arg_lit for -s**

```c
struct arg_lit *symlink_opt =
    arg_lit0("s", "symbolic-link", "make symbolic links instead of copying");
```

Wire up:
```c
.is_symlink = (symlink_opt->count > 0),
```

- [ ] **Step 4: Add symlink logic in copy_file**

In `copy_file`, after the `is_link` block, add:
```c
if (opts->is_symlink) {
    /* symlink() uses the src path literally — for relative symlinks this is fine */
    if (symlink(src, dst) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: cannot create symbolic link\n", dst);
        return -1;
    }
    if (opts->is_verbose) {
        (void)printf("'%s' -> '%s'\n", src, dst);
    }
    return 0;
}
```

In `copy_recursive`, same pattern as -l:
```c
if (opts->is_symlink && S_ISREG(src_stat.st_mode)) {
    return copy_file(src, dst, opts);
}
```

- [ ] **Step 5: Run tests**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -s / --symbolic-link option"
```

---

### Task 8: Add -p / --preserve (preserve file attributes)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

```bash
echo "  ── -p : preserve file attributes ──"
echo "preserve me" > "$TMPDIR"/cp_p_src.txt
# Set specific permissions and timestamp
chmod 641 "$TMPDIR"/cp_p_src.txt
touch -t 202201011234 "$TMPDIR"/cp_p_src.txt
"$MODBOX" cp -p "$TMPDIR"/cp_p_src.txt "$TMPDIR"/cp_p_dst.txt 2>/dev/null || true
src_mode=$(stat -c '%a' "$TMPDIR"/cp_p_src.txt)
dst_mode=$(stat -c '%a' "$TMPDIR"/cp_p_dst.txt)
if [[ "$src_mode" == "$dst_mode" ]]; then
    pass "cp -p → permissions preserved ($src_mode)"
else
    fail "cp -p — permissions differ (src=$src_mode dst=$dst_mode)"
fi
src_mtime=$(stat -c '%Y' "$TMPDIR"/cp_p_src.txt)
dst_mtime=$(stat -c '%Y' "$TMPDIR"/cp_p_dst.txt)
if [[ "$src_mtime" == "$dst_mtime" ]]; then
    pass "cp -p → mtime preserved"
else
    fail "cp -p — mtime differs (src=$src_mtime dst=$dst_mtime)"
fi
```

Expected: FAIL — permissions and mtime will differ.

- [ ] **Step 2: Add `is_preserve` to options struct**

```c
int is_preserve;
```

- [ ] **Step 3: Add arg_lit for -p**

```c
struct arg_lit *preserve_opt =
    arg_lit0("p", "preserve", "preserve file attributes if possible");
```

Wire up.

- [ ] **Step 4: Add preserve logic after copy in copy_file**

At the bottom of `copy_file`, after the fclose calls and before `return 0;`, add:
```c
if (opts->is_preserve) {
    struct stat src_st;
    if (stat(src, &src_st) == 0) {
        /* Preserve permissions */
        (void)fchmod(fileno(fdst), src_st.st_mode & 07777);

        /* Preserve ownership (best effort — may fail if not root) */
        (void)fchown(fileno(fdst), src_st.st_uid, src_st.st_gid);

        /* Preserve timestamps */
        struct timespec times[2];
        times[0] = src_st.st_atim; /* atime */
        times[1] = src_st.st_mtim; /* mtime */
        (void)futimens(fileno(fdst), times);
    }
}
```

Note: fdst is already `fclose`'d at this point. We need to preserve before closing. Restructure: move the preserve block BEFORE the `fclose(fdst)` call, and use `fileno(fdst)` to get the fd.

The flow should be:
```c
/* ... copy loop ... */

if (opts->is_preserve) {
    struct stat src_st;
    if (stat(src, &src_st) == 0) {
        (void)fchmod(fileno(fdst), src_st.st_mode & 07777);
        (void)fchown(fileno(fdst), src_st.st_uid, src_st.st_gid);
        struct timespec times[2];
        times[0] = src_st.st_atim;
        times[1] = src_st.st_mtim;
        (void)futimens(fileno(fdst), times);
    }
}

(void)fclose(fsrc);
(void)fclose(fdst);
return 0;
```

Also need to add includes at the top of `cp.c` if not already present:
```c
#include <sys/stat.h>  /* already included */
/* futimens may need:
   #define _POSIX_C_SOURCE 200809L
   or feature test macros — check if already defined in build system */
```

Check if `futimens` is available. If not, use `utimensat`:
```c
(void)utimensat(AT_FDCWD, dst, times, 0);
```

- [ ] **Step 5: Also preserve for directories in copy_recursive**

In `copy_recursive`, after creating the destination directory and before recursing, add:
```c
if (opts->is_preserve) {
    struct stat src_st;
    if (stat(src, &src_st) == 0) {
        (void)chmod(dst, src_st.st_mode & 07777);
    }
}
```

- [ ] **Step 6: Run tests**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -p / --preserve option"
```

---

### Phase 3: Update and Destination Control (-u, -t, -T)

### Task 9: Add -u / --update (copy only when source is newer)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

```bash
echo "  ── -u : update (source newer) ──"
echo "older" > "$TMPDIR"/cp_u_old.txt
echo "newer" > "$TMPDIR"/cp_u_new.txt
sleep 1  # ensure different mtimes
echo "newer content" > "$TMPDIR"/cp_u_new.txt
"$MODBOX" cp -u "$TMPDIR"/cp_u_old.txt "$TMPDIR"/cp_u_dst.txt 2>/dev/null || true
# dst should get the content of older source (since dst didn't exist)
assert_cmd "older" cat "$TMPDIR"/cp_u_dst.txt

echo "newer real content" > "$TMPDIR"/cp_u_newer.txt
touch -t 202601010000 "$TMPDIR"/cp_u_dst.txt  # make dst very old
"$MODBOX" cp -u "$TMPDIR"/cp_u_newer.txt "$TMPDIR"/cp_u_dst.txt 2>/dev/null || true
assert_cmd "newer real content" cat "$TMPDIR"/cp_u_dst.txt

echo "  ── -u : skip when destination is newer ──"
echo "original dst" > "$TMPDIR"/cp_u_skip_dst.txt
touch -t 202601010000 "$TMPDIR"/cp_u_skip_src.txt  # make source older
"$MODBOX" cp -u "$TMPDIR"/cp_u_skip_src.txt "$TMPDIR"/cp_u_skip_dst.txt 2>/dev/null || true
assert_cmd "original dst" cat "$TMPDIR"/cp_u_skip_dst.txt
```

- [ ] **Step 2: Add `is_update` to options struct**

```c
int is_update;
```

- [ ] **Step 3: Add arg_lit for -u**

```c
struct arg_lit *update_opt =
    arg_lit0("u", "update", "copy only when source is newer than destination");
```

Wire up.

- [ ] **Step 4: Add update logic in copy_file**

At the top of `copy_file`, before any file opens, add:
```c
if (opts->is_update) {
    struct stat src_st, dst_st;
    if (stat(src, &src_st) == 0 && stat(dst, &dst_st) == 0) {
        /* Only copy if source is newer (or same age but we consider 'newer') */
        if (src_st.st_mtime <= dst_st.st_mtime) {
            if (opts->is_verbose) {
                (void)printf("'%s' -> '%s' (skipped: destination is newer)\n", src, dst);
            }
            return 0;
        }
    }
}
```

- [ ] **Step 5: Run tests**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -u / --update option"
```

---

### Task 10: Add -t / --target-directory and -T / --no-target-directory

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing tests**

```bash
echo "  ── -t : target directory ──"
mkdir -p "$TMPDIR"/cp_t_dst
echo "t file" > "$TMPDIR"/cp_t_src.txt
"$MODBOX" cp -t "$TMPDIR"/cp_t_dst "$TMPDIR"/cp_t_src.txt 2>/dev/null || true
assert_cmd "t file" cat "$TMPDIR"/cp_t_dst/cp_t_src.txt

echo "  ── -T : no-target-directory ──"
mkdir -p "$TMPDIR"/cp_T_sub
echo "no T" > "$TMPDIR"/cp_T_src.txt
"$MODBOX" cp -T "$TMPDIR"/cp_T_src.txt "$TMPDIR"/cp_T_sub 2>/dev/null && true
# With -T, if cp_T_sub exists as a directory, cp should error (but here src is a file)
# For this test, verify that -T prevents directory auto-appending
# If cp_T_sub exists as a regular file, cp treats it as dest file
```

- [ ] **Step 2: Add arg_str for -t and arg_lit for -T**

```c
struct arg_str *target_dir_opt =
    arg_str0("t", "target-directory", "DIRECTORY", "copy all SOURCE arguments into DIRECTORY");
struct arg_lit *no_target_dir_opt =
    arg_lit0("T", "no-target-directory", "treat DEST as a normal file (not a directory)");
```

Wire up. Add a `const char *target_dir` local:
```c
const char *explicit_target = (target_dir_opt->count > 0) ? target_dir_opt->sval[0] : NULL;
int is_no_target_dir = (no_target_dir_opt->count > 0);
```

- [ ] **Step 3: Add -t logic to cp_command**

In `cp_command`, after parsing:
```c
if (explicit_target != NULL) {
    /* -t mode: target is from -t, all positional args are sources */
    if (num_files < 1) {
        (void)fprintf(stderr, "cp: missing source operand\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }
    for (int i = 0; i < num_files; i++) {
        const char *src = files_arg->filename[i];
        gchar dest_path[4096];
        gchar *basename = g_path_get_basename(src);
        (void)snprintf(dest_path, sizeof(dest_path), "%s/%s", explicit_target, basename);
        g_free(basename);
        if (opts.is_recursive) {
            copy_recursive(src, dest_path, &opts);
        } else {
            struct stat src_stat;
            if (stat(src, &src_stat) != 0) {
                (void)fprintf(stderr, "cp: %s: No such file or directory\n", src);
                continue;
            }
            if (!S_ISREG(src_stat.st_mode)) {
                (void)fprintf(stderr, "cp: %s: Is not a regular file\n", src);
                continue;
            }
            copy_file(src, dest_path, &opts);
        }
    }
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
}
```

This block goes before the existing `dst = files_arg->filename[num_files - 1]` logic.

- [ ] **Step 4: Add -T logic**

In the existing path, after extracting `dst`:
```c
if (is_no_target_dir) {
    /* Treat dest as a plain file path — do NOT auto-append basename */
    struct stat dst_stat;
    if (stat(dst, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: -T: cannot override directory '%s'\n", dst);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }
    for (int i = 0; i < num_srcs; i++) {
        const char *src = files_arg->filename[i];
        if (opts.is_recursive) {
            copy_recursive(src, dst, &opts);
        } else {
            copy_file(src, dst, &opts);
        }
    }
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
}
```

- [ ] **Step 5: Run tests**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS (note: the -T test may need adjustment depending on exact behavior).

- [ ] **Step 6: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -t / --target-directory and -T / --no-target-directory"
```

---

### Phase 4: Symlink Handling (-L, -P)

### Task 11: Add -L / --dereference and -P / --no-dereference

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

```bash
echo "  ── -P : no-dereference (copy symlink itself) ──"
echo "target content" > "$TMPDIR"/cp_P_target.txt
ln -s "$TMPDIR"/cp_P_target.txt "$TMPDIR"/cp_P_link
"$MODBOX" cp -P "$TMPDIR"/cp_P_link "$TMPDIR"/cp_P_copy 2>/dev/null || true
if [[ -L "$TMPDIR"/cp_P_copy ]]; then
    pass "cp -P → symlink preserved (copy is a symlink)"
else
    fail "cp -P — copy is not a symlink"
fi
```

Expected: FAIL — currently `stat()` dereferences symlinks.

- [ ] **Step 2: Add `is_dereference` and `is_no_dereference` to options struct**

```c
int is_dereference;      /* -L: follow symlinks */
int is_no_dereference;   /* -P: don't follow symlinks */
```

Actually simpler: just have:
```c
int follow_symlinks;  /* 1 = follow (default for non-recursive? No, default is follow for all),
                       * 0 = don't follow (-P).
                       * Actually GNU cp default: without -r, always dereference.
                       * With -r, follow unless -P given. With -L, always follow. */
```

GNU coreutils cp behavior:
- Default (no -r): always dereference symlinks in source
- `-P` / `--no-dereference`: never follow symlinks (copy symlink as symlink)
- `-L` / `--dereference`: always follow symlinks in source

Since default currently is always-dereference (uses `stat()`), `-P` is the main new behavior. Let's add both:

```c
int is_dereference;      /* -L: follow symlinks */
int is_no_dereference;   /* -P: don't follow symlinks */
```

- [ ] **Step 3: Add arg_lit entries**

```c
struct arg_lit *deref_opt =
    arg_lit0("L", "dereference", "always follow symbolic links in SOURCE");
struct arg_lit *no_deref_opt =
    arg_lit0("P", "no-dereference", "never follow symbolic links in SOURCE");
```

Wire up:
```c
.is_dereference = (deref_opt->count > 0),
.is_no_dereference = (no_deref_opt->count > 0),
```

Add to argtable.

- [ ] **Step 4: Modify copy_recursive to use lstat when -P is set**

In `copy_recursive`, change the initial `stat()` call:

```c
struct stat src_stat;
int stat_result;
if (opts->is_no_dereference) {
    stat_result = lstat(src, &src_stat);
} else {
    stat_result = stat(src, &src_stat);
}
```

Add handling for symlinks when `-P` is set. In the file-type switch, after the directory check and before the "not a regular file" error:

```c
if (opts->is_no_dereference && S_ISLNK(src_stat.st_mode)) {
    /* Copy the symlink itself */
    char link_target[4096];
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
    if (len == -1) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: Cannot read link\n", src);
        return -1;
    }
    link_target[len] = '\0';
    if (symlink(link_target, dst) != 0) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "cp: %s: Cannot create symbolic link\n", dst);
        return -1;
    }
    if (opts->is_verbose) {
        (void)printf("'%s' -> '%s'\n", src, dst);
    }
    return 0;
}
```

Also need to add `#include <unistd.h>` for `readlink` / `symlink` if not already there.

- [ ] **Step 5: Run tests**

```bash
make && bash tests/run_tests.sh
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/commands/cp.c tests/run_tests.sh
git commit -m "feat(cp): add -L / --dereference and -P / --no-dereference"
```

---

### Phase 5: Directory Structure Preservation (--parents)

### Task 12: Add --parents (preserve directory structure in destination)

**Files:**
- Modify: `src/commands/cp.c`
- Modify: `tests/run_tests.sh`

- [ ] **Step 1: Write failing test**

```bash
echo "  ── --parents : preserve directory structure ──"
mkdir -p "$TMPDIR"/cp_parents/a/b
echo "deep file" > "$TMPDIR"/cp_parents/a/b/file.txt
mkdir -p "$TMPDIR"/cp_parents_dst
# cp --parents -r parents/a/b/file.txt parents_dst/
# should create parents_dst/parents/a/b/file.txt
"$MODBOX" cp --parents -r "$TMPDIR"/cp_parents/a/b/file.txt "$TMPDIR"/cp_parents_dst/ 2>/dev/null || true
assert_cmd "deep file" cat "$TMPDIR"/cp_parents_dst/"$TMPDIR"/cp_parents/a/b/file.txt
```

Actually the semantics of `--parents` need careful matching. GNU `cp --parents src/file dir/` creates `dir/src/file`. The path after the last `..` or the leading `/` is preserved. Let me adjust:

```bash
echo "  ── --parents : preserve directory structure ──"
mkdir -p "$TMPDIR"/cp_parents_sub/a/b
echo "deep" > "$TMPDIR"/cp_parents_sub/a/b/f.txt
mkdir -p "$TMPDIR"/cp_parents_out
"$MODBOX" cp --parents -r "$TMPDIR"/cp_parents_sub/a/b/f.txt "$TMPDIR"/cp_parents_out/ 2>/dev/null || true
# expected: cp_parents_out/<full absolute path>/a/b/f.txt
# Hmm, that's unwieldy. Let's test with relative paths.
```

Actually, for simplicity, let's skip this task — `--parents` is complex and rarely used in absolute-path contexts. It's better left for a later expansion.

**Decision: Remove this task from the plan.** The --parents feature is complex and low-value. Tasks 1-11 already provide the critical mass of cp functionality.

---

## Verification

After all tasks are complete, run the full test suite:

```bash
make clean && make && bash tests/run_tests.sh
```

Expected: All ~100+ tests pass (existing + new cp tests), exit code 0.

## Summary of New Options

| Option | Task | Behavior |
|--------|------|----------|
| `-n` | 2 | Skip if destination exists |
| `-f` | 3 | Remove destination before writing |
| `-i` | 4 | Prompt before overwrite |
| `-R` | 5 | Alias for `-r` |
| `-l` | 6 | Create hard link instead of copy |
| `-s` | 7 | Create symbolic link instead of copy |
| `-p` | 8 | Preserve mode, ownership, timestamps |
| `-u` | 9 | Copy only when source is newer |
| `-t` | 10 | Specify target directory explicitly |
| `-T` | 10 | Treat DEST as normal file, not directory |
| `-L` | 11 | Follow symbolic links in source |
| `-P` | 11 | Don't follow symbolic links in source |
| `-b` / `--backup` | — | (deferred — requires backup numbering logic) |
| `--sparse` | — | (deferred — requires file system level features) |
| `--parents` | — | (deferred — complex path prefix logic) |
