#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ftw.h>
#include <grp.h>
#include <pwd.h>
#include <acl/libacl.h>
#include "commands/setfacl.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

/* ── helpers ───────────────────────────────────────────────────────────── */

static int parse_perms(const char *str, acl_permset_t permset) {
    if (acl_clear_perms(permset) != 0) return -1;
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case 'r': if (acl_add_perm(permset, ACL_READ) != 0) return -1; break;
            case 'w': if (acl_add_perm(permset, ACL_WRITE) != 0) return -1; break;
            case 'x': if (acl_add_perm(permset, ACL_EXECUTE) != 0) return -1; break;
            case '-': break;
            case 'R': if (acl_add_perm(permset, ACL_READ) != 0) return -1; break;
            case 'W': if (acl_add_perm(permset, ACL_WRITE) != 0) return -1; break;
            case 'X': if (acl_add_perm(permset, ACL_EXECUTE) != 0) return -1; break;
            default:  return -1;
        }
    }
    return 0;
}

/* Resolve a user name/ID string to uid_t. Returns -1 on failure. */
static uid_t resolve_user(const char *name) {
    struct passwd *pw = getpwnam(name);
    if (pw) return pw->pw_uid;
    char *end = nullptr;
    uid_t uid = (uid_t)strtoul(name, &end, 10);
    if (*end == '\0' && end != name) return uid;
    return (uid_t)-1;
}

/* Resolve a group name/ID string to gid_t. Returns -1 on failure. */
static gid_t resolve_group(const char *name) {
    struct group *gr = getgrnam(name);
    if (gr) return gr->gr_gid;
    char *end = nullptr;
    gid_t gid = (gid_t)strtoul(name, &end, 10);
    if (*end == '\0' && end != name) return gid;
    return (gid_t)-1;
}

/* ── entry parsing ─────────────────────────────────────────────────────── */

struct ParsedEntry {
    acl_tag_t tag;
    int is_default;
    int valid;
    char qualifier[256];
    char perms[8];
};

static ParsedEntry parse_entry(const char *spec) {
    ParsedEntry e = {};
    e.valid = 0;
    if (!spec || !*spec) return e;

    const char *p = spec;

    /* Check for d: prefix */
    if (strncmp(p, "d:", 2) == 0) {
        e.is_default = 1;
        p += 2;
    }

    /* Tag type */
    if (*p == 'u')      { e.tag = ACL_USER;  }
    else if (*p == 'g') { e.tag = ACL_GROUP; }
    else if (*p == 'o') { e.tag = ACL_OTHER; }
    else if (*p == 'm') { e.tag = ACL_MASK;  }
    else { return e; }
    p++;

    if (*p != ':') return e;
    p++;

    /* Qualifier (everything up to next ':' or end) */
    const char *qs = p;
    while (*p && *p != ':') p++;
    size_t ql = (size_t)(p - qs);
    if (ql >= sizeof(e.qualifier)) return e;
    memcpy(e.qualifier, qs, ql);
    e.qualifier[ql] = '\0';

    /* Permissions (after ':' if present) */
    if (*p == ':') {
        p++;
        const char *ps = p;
        while (*p && *p != ',') p++;
        size_t pl = (size_t)(p - ps);
        if (pl >= sizeof(e.perms)) return e;
        memcpy(e.perms, ps, pl);
        e.perms[pl] = '\0';
    }

    e.valid = 1;
    return e;
}

/* Split comma-separated entries from a single -m / -x value.
   Pushes each entry string into the provided vector. */
static void split_entries(const char *arg, std::vector<std::string> &out) {
    const char *start = arg;
    while (*start) {
        const char *end = start;
        while (*end && *end != ',') end++;
        if (end > start) {
            out.emplace_back(start, (size_t)(end - start));
        }
        start = (*end == ',') ? end + 1 : end;
    }
}

/* ── ACL manipulation ──────────────────────────────────────────────────── */

/* Apply one modify entry to an existing ACL. */
static int apply_modify_entry(acl_t acl, const ParsedEntry &entry) {
    acl_entry_t match;
    int found = 0;

    /* Search for existing entry with same tag + qualifier */
    for (int id = ACL_FIRST_ENTRY; ; id = ACL_NEXT_ENTRY) {
        acl_entry_t ent;
        if (acl_get_entry(acl, id, &ent) != 1) break;
        acl_tag_t tag;
        if (acl_get_tag_type(ent, &tag) != 0) continue;
        if (tag != entry.tag) continue;

        if (tag == ACL_USER) {
            void *q = acl_get_qualifier(ent);
            if (!q) continue;
            uid_t existing = *(uid_t *)q;
            uid_t target = resolve_user(entry.qualifier);
            acl_free(q);
            if (existing == target) { match = ent; found = 1; break; }
        } else if (tag == ACL_GROUP) {
            void *q = acl_get_qualifier(ent);
            if (!q) continue;
            gid_t existing = *(gid_t *)q;
            gid_t target = resolve_group(entry.qualifier);
            acl_free(q);
            if (existing == target) { match = ent; found = 1; break; }
        } else {
            /* MASK, OTHER — first match */
            match = ent;
            found = 1;
            break;
        }
    }

    if (!found) {
        if (acl_create_entry(&acl, &match) != 0) return -1;
        if (acl_set_tag_type(match, entry.tag) != 0) return -1;
        if (entry.tag == ACL_USER) {
            uid_t uid = resolve_user(entry.qualifier);
            if (uid == (uid_t)-1) {
                fprintf(stderr, "setfacl: invalid user '%s'\n", entry.qualifier);
                return -1;
            }
            if (acl_set_qualifier(match, &uid) != 0) return -1;
        } else if (entry.tag == ACL_GROUP) {
            gid_t gid = resolve_group(entry.qualifier);
            if (gid == (gid_t)-1) {
                fprintf(stderr, "setfacl: invalid group '%s'\n", entry.qualifier);
                return -1;
            }
            if (acl_set_qualifier(match, &gid) != 0) return -1;
        }
    }

    /* Set permissions if provided */
    if (entry.perms[0] != '\0') {
        acl_permset_t permset;
        if (acl_get_permset(match, &permset) != 0) return -1;
        if (parse_perms(entry.perms, permset) != 0) {
            fprintf(stderr, "setfacl: invalid permissions '%s'\n", entry.perms);
            return -1;
        }
        if (acl_set_permset(match, permset) != 0) return -1;
    }

    return 0;
}

/* Apply one remove spec to an existing ACL. */
static int apply_remove_entry(acl_t acl, const ParsedEntry &entry) {
    for (int id = ACL_FIRST_ENTRY; ; id = ACL_NEXT_ENTRY) {
        acl_entry_t ent;
        if (acl_get_entry(acl, id, &ent) != 1) break;
        acl_tag_t tag;
        if (acl_get_tag_type(ent, &tag) != 0) continue;
        if (tag != entry.tag) continue;

        int matched = 0;
        if (tag == ACL_USER) {
            void *q = acl_get_qualifier(ent);
            if (!q) continue;
            uid_t existing = *(uid_t *)q;
            uid_t target = resolve_user(entry.qualifier);
            acl_free(q);
            if (existing == target) matched = 1;
        } else if (tag == ACL_GROUP) {
            void *q = acl_get_qualifier(ent);
            if (!q) continue;
            gid_t existing = *(gid_t *)q;
            gid_t target = resolve_group(entry.qualifier);
            acl_free(q);
            if (existing == target) matched = 1;
        } else {
            /* MASK, OTHER — no qualifier check */
            matched = 1;
        }

        if (matched) {
            if (acl_delete_entry(acl, ent) != 0) return -1;
            return 0;
        }
    }
    /* Entry not found is not an error */
    return 0;
}

/* Remove all extended entries (USER, GROUP, MASK) from an ACL,
   keeping only USER_OBJ, GROUP_OBJ, OTHER. */
static int apply_remove_all(acl_t acl) {
    int restarted = 1;
    while (restarted) {
        restarted = 0;
        for (int id = ACL_FIRST_ENTRY; ; id = ACL_NEXT_ENTRY) {
            acl_entry_t ent;
            if (acl_get_entry(acl, id, &ent) != 1) break;
            acl_tag_t tag;
            if (acl_get_tag_type(ent, &tag) != 0) continue;
            if (tag == ACL_USER || tag == ACL_GROUP || tag == ACL_MASK) {
                acl_delete_entry(acl, ent);
                restarted = 1;
                break;  /* restart iteration */
            }
        }
    }
    return 0;
}

/* Read all entries from a file (-M or -X mode). */
static std::vector<std::string> read_entries_file(const char *path) {
    std::vector<std::string> entries;
    FILE *f = stdin;
    bool close_file = false;

    if (strcmp(path, "-") != 0) {
        f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
            return entries;
        }
        close_file = true;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Trim trailing newline and whitespace */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'
                       || line[len - 1] == ' '  || line[len - 1] == '\t'))
            line[--len] = '\0';
        /* Skip empty lines and comments */
        if (len == 0 || line[0] == '#') continue;
        entries.emplace_back(line);
    }

    if (close_file) fclose(f);
    return entries;
}

/* ── single-file processing ────────────────────────────────────────────── */

static int process_one_file(const char *path, const SetfaclOptions *opts) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
        return 1;
    }

    int errors = 0;

    /* ── --set / --set-file: replace access ACL ── */
    const char *set_text = nullptr;
    std::string set_buf;

    if (opts->set_file) {
        std::vector<std::string> lines = read_entries_file(opts->set_file);
        for (const auto &l : lines) {
            if (!set_buf.empty()) set_buf += "\n";
            set_buf += l;
        }
        set_text = set_buf.c_str();
    } else if (opts->set_acl) {
        set_text = opts->set_acl;
    }

    if (set_text) {
        acl_t acl = acl_from_text(set_text);
        if (!acl) {
            fprintf(stderr, "setfacl: invalid ACL specification\n");
            return 1;
        }
        if (acl_valid(acl) != 0) {
            fprintf(stderr, "setfacl: %s: ACL is not valid\n", path);
            acl_free(acl);
            return 1;
        }
        if (!opts->is_test_mode) {
            if (acl_set_file(path, ACL_TYPE_ACCESS, acl) != 0) {
                fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
                errors = 1;
            }
        } else {
            char *text = acl_to_any_text(acl, nullptr, '\n', TEXT_ABBREVIATE);
            if (text) {
                printf("%s\n", text);
                acl_free(text);
            }
        }
        acl_free(acl);
        /* --set replaces ACL; fall through to handle -m/-x on top */
    }

    /* ── Collect modify entries ── */
    std::vector<ParsedEntry> access_mods, def_mods;
    std::vector<ParsedEntry> access_rems, def_rems;

    auto collect = [&](const std::vector<const char *> &specs,
                       std::vector<ParsedEntry> &acc,
                       std::vector<ParsedEntry> &def) {
        for (const char *spec : specs) {
            std::vector<std::string> parts;
            split_entries(spec, parts);
            for (const auto &part : parts) {
                ParsedEntry e = parse_entry(part.c_str());
                if (!e.valid) {
                    fprintf(stderr, "setfacl: invalid ACL entry: '%s'\n", part.c_str());
                    continue;
                }
                if (e.is_default) def.push_back(e);
                else acc.push_back(e);
            }
        }
    };

    collect(opts->modify_entries, access_mods, def_mods);
    collect(opts->remove_specs, access_rems, def_rems);

    /* Read entries from -M / -X files */
    if (opts->modify_file) {
        auto lines = read_entries_file(opts->modify_file);
        for (const auto &line : lines) {
            ParsedEntry e = parse_entry(line.c_str());
            if (!e.valid) {
                fprintf(stderr, "setfacl: invalid ACL entry: '%s'\n", line.c_str());
                continue;
            }
            if (e.is_default) def_mods.push_back(e);
            else access_mods.push_back(e);
        }
    }

    if (opts->remove_file) {
        auto lines = read_entries_file(opts->remove_file);
        for (const auto &line : lines) {
            ParsedEntry e = parse_entry(line.c_str());
            if (!e.valid) {
                fprintf(stderr, "setfacl: invalid ACL entry: '%s'\n", line.c_str());
                continue;
            }
            if (e.is_default) def_rems.push_back(e);
            else access_rems.push_back(e);
        }
    }

    /* ── Access ACL (apply -m, -x, -b) ── */
    bool has_access_ops = !access_mods.empty() || !access_rems.empty() || opts->remove_all;
    if (has_access_ops) {
        acl_t acl = acl_get_file(path, ACL_TYPE_ACCESS);
        if (!acl) {
            fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
            return 1;
        }

        for (auto &e : access_mods) {
            if (apply_modify_entry(acl, e) != 0) errors = 1;
        }
        for (auto &e : access_rems) {
            if (apply_remove_entry(acl, e) != 0) errors = 1;
        }
        if (opts->remove_all) {
            if (apply_remove_all(acl) != 0) errors = 1;
        }

        if (opts->recalc_mask) {
            acl_calc_mask(&acl);
        }

        if (!opts->is_test_mode) {
            if (acl_set_file(path, ACL_TYPE_ACCESS, acl) != 0) {
                fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
                errors = 1;
            }
        } else {
            char *text = acl_to_any_text(acl, nullptr, '\n', TEXT_ABBREVIATE);
            if (text) {
                printf("%s\n", text);
                acl_free(text);
            }
        }
        acl_free(acl);
    }

    /* ── Default ACL (apply d:-prefixed entries) ── */
    bool has_def_ops = !def_mods.empty() || !def_rems.empty();
    if (has_def_ops) {
        acl_t acl = acl_get_file(path, ACL_TYPE_DEFAULT);
        /* acl_get_file may return non-NULL when no default ACL exists;
         * check if the ACL is actually empty/invalid */
        bool need_seed = true;
        if (acl) {
            /* Check if ACL has at least one entry (not an empty shell) */
            acl_entry_t probe;
            if (acl_get_entry(acl, ACL_FIRST_ENTRY, &probe) == 1)
                need_seed = false;
        }
        if (need_seed) {
            if (acl) acl_free(acl);
            /* Seed default ACL from access ACL base entries */
            acl_t acc_acl = acl_get_file(path, ACL_TYPE_ACCESS);
            if (!acc_acl) {
                fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
                return 1;
            }
            acl = acl_init(3);
            if (!acl) {
                acl_free(acc_acl);
                fprintf(stderr, "setfacl: %s: cannot init ACL\n", path);
                return 1;
            }
            acl_entry_t src_ent, dst_ent;
            for (int id = ACL_FIRST_ENTRY; ; id = ACL_NEXT_ENTRY) {
                if (acl_get_entry(acc_acl, id, &src_ent) != 1) break;
                acl_tag_t tag;
                if (acl_get_tag_type(src_ent, &tag) != 0) continue;
                if (tag != ACL_USER_OBJ && tag != ACL_GROUP_OBJ && tag != ACL_OTHER)
                    continue;
                if (acl_create_entry(&acl, &dst_ent) != 0) continue;
                acl_copy_entry(dst_ent, src_ent);
            }
            acl_free(acc_acl);
        }

        for (auto &e : def_mods) {
            if (apply_modify_entry(acl, e) != 0) errors = 1;
        }
        for (auto &e : def_rems) {
            if (apply_remove_entry(acl, e) != 0) errors = 1;
        }

        if (opts->recalc_mask) {
            acl_calc_mask(&acl);
        }

        if (!opts->is_test_mode) {
            if (acl_set_file(path, ACL_TYPE_DEFAULT, acl) != 0) {
                fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
                errors = 1;
            }
        }
        acl_free(acl);
    }

    /* ── -k: remove default ACL ── */
    if (opts->remove_default) {
        if (!opts->is_test_mode) {
            acl_delete_def_file(path);
        }
    }

    return errors ? 1 : 0;
}

/* ── --restore ─────────────────────────────────────────────────────────── */

static int process_restore(const char *backup_path, const SetfaclOptions *opts) {
    FILE *f = fopen(backup_path, "r");
    if (!f) {
        fprintf(stderr, "setfacl: %s: %s\n", backup_path, strerror(errno));
        return 1;
    }

    int errors = 0;
    char line[4096];
    std::string cur_path;
    std::string cur_acl_text;

    auto apply_cur = [&]() {
        if (cur_path.empty() || cur_acl_text.empty()) return;
        acl_t acl = acl_from_text(cur_acl_text.c_str());
        if (!acl) {
            fprintf(stderr, "setfacl: %s: invalid ACL in restore file\n", cur_path.c_str());
            errors = 1;
            cur_path.clear();
            cur_acl_text.clear();
            return;
        }
        if (!opts->is_test_mode) {
            if (acl_set_file(cur_path.c_str(), ACL_TYPE_ACCESS, acl) != 0) {
                fprintf(stderr, "setfacl: %s: %s\n", cur_path.c_str(), strerror(errno));
                errors = 1;
            }
        }
        acl_free(acl);
        cur_path.clear();
        cur_acl_text.clear();
    };

    bool in_block = false;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        if (strncmp(line, "# file: ", 8) == 0) {
            /* New file block — apply previous one first */
            apply_cur();
            cur_path = line + 8;
            in_block = true;
        } else if (!in_block) {
            /* Skip everything before the first # file: line */
            continue;
        } else if (line[0] == '#') {
            /* Skip other comments (# owner:, # group:) */
            continue;
        } else if (len > 0) {
            /* ACL entry line — accumulate */
            if (!cur_acl_text.empty()) cur_acl_text += "\n";
            cur_acl_text += line;
        }
    }
    /* Apply the last block */
    apply_cur();

    fclose(f);
    return errors ? 1 : 0;
}

/* ── recursive traversal ───────────────────────────────────────────────── */

static const SetfaclOptions *g_rec_opts;
static int g_rec_errors;

static int rec_callback(const char *fpath, const struct stat *sb,
                         int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (typeflag == FTW_NS || typeflag == FTW_DNR) {
        fprintf(stderr, "setfacl: %s: %s\n", fpath, strerror(errno));
        g_rec_errors = 1;
        return 0;
    }

    if (process_one_file(fpath, g_rec_opts) != 0) {
        g_rec_errors = 1;
    }
    return 0;
}

/* ── command entry point ───────────────────────────────────────────────── */

int setfacl_command(int argc, char **argv) {
    struct arg_lit *help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit *version_opt = arg_lit0("v", "version", "output version info");
    struct arg_str *modify_opt =
        arg_strn("m", "modify", "ACL", 0, 100, "modify the ACL of file(s)");
    struct arg_str *modify_file_opt =
        arg_str0("M", "modify-file", "FILE", "read ACL entries to modify from file");
    struct arg_str *remove_opt =
        arg_strn("x", "remove", "ACL", 0, 100, "remove ACL entries");
    struct arg_str *remove_file_opt =
        arg_str0("X", "remove-file", "FILE", "read ACL entries to remove from file");
    struct arg_lit *remove_all_opt =
        arg_lit0("b", "remove-all", "remove all extended ACL entries");
    struct arg_lit *remove_default_opt =
        arg_lit0("k", "remove-default", "remove the default ACL");
    struct arg_str *set_opt =
        arg_str0(nullptr, "set", "ACL", "set the ACL of file(s), replacing the current ACL");
    struct arg_str *set_file_opt =
        arg_str0(nullptr, "set-file", "FILE", "read ACL entries to set from file");
    struct arg_str *restore_opt =
        arg_str0(nullptr, "restore", "FILE", "restore a permission backup created by getfacl");
    struct arg_lit *no_mask_opt =
        arg_lit0("n", "no-mask", "don't recalculate the effective rights mask");
    struct arg_lit *test_opt =
        arg_lit0(nullptr, "test", "test mode (ACLs are not modified)");
    struct arg_lit *recursive_opt =
        arg_lit0("R", "recursive", "apply operations to all files and directories recursively");
    struct arg_lit *physical_opt =
        arg_litn("P", "physical", 0, 1, "do not follow symbolic links (default)");
    struct arg_lit *logical_opt =
        arg_litn("L", "logical", 0, 1, "follow all symbolic links");
    struct arg_lit *dereference_opt =
        arg_litn("H", "dereference", 0, 1, "dereference command-line symbolic links");
    struct arg_lit *preserve_root_opt =
        arg_lit0(nullptr, "preserve-root", "fail to operate recursively on '/'");
    struct arg_lit *one_fs_opt =
        arg_lit0(nullptr, "one-file-system", "stay within one filesystem");
    struct arg_file *files =
        arg_filen(nullptr, nullptr, "FILE...", 0, 1000, "files to operate on");
    struct arg_end *end = arg_end(30);

    ArgTable at({help_opt, version_opt, modify_opt, modify_file_opt,
                 remove_opt, remove_file_opt, remove_all_opt,
                 remove_default_opt, set_opt, set_file_opt, restore_opt,
                 no_mask_opt, test_opt, recursive_opt, physical_opt,
                 logical_opt, dereference_opt, preserve_root_opt,
                 one_fs_opt, files, end});

    int nerrors = at.parse(argc, argv);

    if (version_opt->count > 0) {
        printf("modbox setfacl (modbox)\n");
        printf("Copyright (C) Free Software Foundation. License GPLv3+\n");
        printf("This is free software: you are free to change and redistribute it.\n");
        printf("There is NO WARRANTY, to the extent permitted by law.\n");
        return 0;
    }

    if (help_opt->count > 0) {
        printf("Usage: setfacl [-bkndRLPvh] [{-m|-x} acl_spec] [{-M|-X} acl_file]\n");
        printf("                [--set acl_spec] [--set-file acl_file]\n");
        printf("                [--restore restore_file] file ...\n");
        printf("Set file access control lists\n");
        printf("\n");
        printf("  -m, --modify=ACL        modify the current ACL entry on file(s)\n");
        printf("  -M, --modify-file=FILE  read ACL entries to modify from file\n");
        printf("  -x, --remove=ACL        remove ACL entries\n");
        printf("  -X, --remove-file=FILE  read ACL entries to remove from file\n");
        printf("  -b, --remove-all        remove all extended ACL entries\n");
        printf("  -k, --remove-default    remove the default ACL\n");
        printf("      --set=ACL           set the ACL of file(s), replacing current ACL\n");
        printf("      --set-file=FILE     read ACL entries to set from file\n");
        printf("      --restore=FILE      restore a permission backup (from getfacl -R)\n");
        printf("  -n, --no-mask           don't recalculate the effective rights mask\n");
        printf("      --test              test mode (ACLs are not actually changed)\n");
        printf("  -R, --recursive         apply operations recursively\n");
        printf("  -L, --logical           logical walk, follow symbolic links\n");
        printf("  -P, --physical          physical walk, do not follow symbolic links (default)\n");
        printf("  -H, --dereference       dereference command-line symbolic links\n");
        printf("      --preserve-root     fail to operate recursively on '/'\n");
        printf("      --one-file-system   skip files on different filesystems\n");
        printf("  -v, --version           output version information\n");
        printf("  -h, --help              display this help and exit\n");
        printf("\n");
        printf("ACL entry format: [d:]<type>:<qualifier>:<perms>\n");
        printf("  d:      default ACL (directories only)\n");
        printf("  type:   u (user), g (group), o (other), m (mask)\n");
        printf("  perms:  r (read), w (write), x (execute)\n");
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    /* ── Check for missing operand ── */
    bool has_operation = (modify_opt->count > 0 || remove_opt->count > 0
                       || modify_file_opt->count > 0 || remove_file_opt->count > 0
                       || remove_all_opt->count > 0 || remove_default_opt->count > 0
                       || set_opt->count > 0 || set_file_opt->count > 0
                       || restore_opt->count > 0);

    if (restore_opt->count == 0 && files->count == 0) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    if (!has_operation) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    /* ── Populate options ── */
    SetfaclOptions opts;
    opts.is_recursive = (recursive_opt->count > 0);
    opts.is_test_mode = (test_opt->count > 0);
    opts.no_mask = (no_mask_opt->count > 0);
    opts.recalc_mask = opts.no_mask ? 0 : 1;
    opts.is_logical = (logical_opt->count > 0);
    opts.is_physical = opts.is_logical ? 0 : 1;
    opts.is_dereference = (dereference_opt->count > 0);
    opts.preserve_root = (preserve_root_opt->count > 0);
    opts.one_file_system = (one_fs_opt->count > 0);
    opts.remove_all = (remove_all_opt->count > 0);
    opts.remove_default = (remove_default_opt->count > 0);

    if (set_opt->count > 0)       opts.set_acl    = set_opt->sval[0];
    if (set_file_opt->count > 0)  opts.set_file   = set_file_opt->sval[0];
    if (restore_opt->count > 0)   opts.restore_file = restore_opt->sval[0];
    if (modify_file_opt->count > 0) opts.modify_file = modify_file_opt->sval[0];
    if (remove_file_opt->count > 0) opts.remove_file = remove_file_opt->sval[0];

    for (int i = 0; i < modify_opt->count; i++)
        opts.modify_entries.push_back(modify_opt->sval[i]);
    for (int i = 0; i < remove_opt->count; i++)
        opts.remove_specs.push_back(remove_opt->sval[i]);

    /* ── --restore (special multi-file path) ── */
    if (opts.restore_file) {
        return process_restore(opts.restore_file, &opts);
    }

    /* ── Build nftw flags ── */
    int nftw_flags = 0;
    if (!opts.is_logical) nftw_flags |= FTW_PHYS;
    if (opts.is_recursive) nftw_flags |= FTW_DEPTH;
#ifdef FTW_MOUNT
    if (opts.one_file_system) nftw_flags |= FTW_MOUNT;
#endif

    /* ── Process files ── */
    g_rec_opts = &opts;
    g_rec_errors = 0;

    int n = files->count;
    for (int i = 0; i < n; i++) {
        const char *path = files->filename[i];

        if (opts.is_dereference) {
            struct stat lst;
            if (lstat(path, &lst) == 0 && S_ISLNK(lst.st_mode)) {
                static char resolved[4096];
                if (realpath(path, resolved)) path = resolved;
            }
        }

        if (opts.is_recursive) {
            if (opts.preserve_root && strcmp(path, "/") == 0) {
                fprintf(stderr, "setfacl: it is dangerous to operate recursively on '/'\n");
                fprintf(stderr, "setfacl: use --no-preserve-root to override this failsafe\n");
                g_rec_errors = 1;
                continue;
            }
            if (nftw(path, rec_callback, 20, nftw_flags) != 0) {
                fprintf(stderr, "setfacl: %s: %s\n", path, strerror(errno));
                g_rec_errors = 1;
            }
        } else {
            if (process_one_file(path, &opts) != 0) {
                g_rec_errors = 1;
            }
        }
    }

    return g_rec_errors ? 1 : 0;
}

REGISTER_COMMAND("setfacl", setfacl_command, "Set file access control lists")
