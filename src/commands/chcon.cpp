#include <argtable3.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ftw.h>
#include <string>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <unistd.h>

#include "commands/chcon.hpp"
#include "commands/command_macros.hpp"

struct ChconOptions {
    int is_recursive = 0;
    int is_verbose = 0;
    int no_dereference = 0;
    int preserve_root = 0;
    const char* user = nullptr;
    const char* role = nullptr;
    const char* type = nullptr;
    const char* range = nullptr;
    const char* reference = nullptr;
};

static int chcon_errors = 0;

static std::string build_context(const char* current_ctx_str,
                                  const ChconOptions* opts,
                                  const char* ref_ctx_str) {
    const char* base_ctx = ref_ctx_str ? ref_ctx_str : current_ctx_str;

    context_t ctx = context_new(base_ctx);
    if (!ctx) return "";

    if (opts->user) {
        if (context_user_set(ctx, opts->user) != 0) {
            context_free(ctx);
            return "";
        }
    }
    if (opts->role) {
        if (context_role_set(ctx, opts->role) != 0) {
            context_free(ctx);
            return "";
        }
    }
    if (opts->type) {
        if (context_type_set(ctx, opts->type) != 0) {
            context_free(ctx);
            return "";
        }
    }
    if (opts->range) {
        if (context_range_set(ctx, opts->range) != 0) {
            context_free(ctx);
            return "";
        }
    }

    const char* result = context_str(ctx);
    std::string out = result ? result : "";
    context_free(ctx);
    return out;
}

static int chcon_one_file(const char* path, const ChconOptions* opts) {
    char* current_ctx = nullptr;

    int get_rc;
    if (opts->no_dereference) {
        get_rc = lgetfilecon(path, &current_ctx);
    } else {
        get_rc = getfilecon(path, &current_ctx);
    }

    if (get_rc < 0) {
        if (errno != ENOTSUP) {
            fprintf(stderr, "chcon: failed to get context of '%s': %s\n",
                    path, strerror(errno));
        } else {
            fprintf(stderr, "chcon: '%s' has no security context\n", path);
        }
        return 1;
    }

    const char* ref_ctx_str = nullptr;
    std::string ref_ctx_owned;

    if (opts->reference) {
        char* ref_ctx = nullptr;
        int ref_rc = getfilecon(opts->reference, &ref_ctx);
        if (ref_rc < 0) {
            fprintf(stderr, "chcon: failed to get context of '%s': %s\n",
                    opts->reference, strerror(errno));
            freecon(current_ctx);
            return 1;
        }
        ref_ctx_owned = ref_ctx;
        ref_ctx_str = ref_ctx_owned.c_str();
        freecon(ref_ctx);
    }

    std::string new_ctx = build_context(current_ctx, opts, ref_ctx_str);
    freecon(current_ctx);

    if (new_ctx.empty()) {
        fprintf(stderr, "chcon: failed to construct new context for '%s'\n", path);
        return 1;
    }

    int set_rc;
    if (opts->no_dereference) {
        set_rc = lsetfilecon(path, new_ctx.c_str());
    } else {
        set_rc = setfilecon(path, new_ctx.c_str());
    }

    if (set_rc < 0) {
        fprintf(stderr, "chcon: failed to change context of '%s' to '%s': %s\n",
                path, new_ctx.c_str(), strerror(errno));
        return 1;
    }

    if (opts->is_verbose) {
        printf("changed context of '%s' to '%s'\n", path, new_ctx.c_str());
    }

    return 0;
}

// ── nftw callback ─────────────────────────────────────────────────────────
static const ChconOptions* chcon_glob_opts;

static int recursive_callback(const char* fpath, const struct stat* sb,
                               int typeflag, struct FTW* ftwbuf) {
    (void)sb;
    (void)ftwbuf;

    if (chcon_glob_opts->preserve_root && strcmp(fpath, "/") == 0) {
        fprintf(stderr, "chcon: it is dangerous to operate recursively on '/'\n");
        fprintf(stderr, "chcon: use --no-preserve-root to override this failsafe\n");
        chcon_errors = 1;
        return 0;
    }

    if (chcon_one_file(fpath, chcon_glob_opts) != 0) {
        chcon_errors = 1;
    }
    return 0;
}

void chcon_command(int argc, char** argv) {
    struct arg_lit* recursive_opt = arg_lit0("R", "recursive", "operate on files and directories recursively");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "output a diagnostic for every file processed");
    struct arg_lit* no_deref_opt = arg_lit0("h", "no-dereference", "affect symbolic links instead of their target");
    struct arg_lit* preserve_root_opt = arg_lit0(NULL, "preserve-root", "fail to operate recursively on '/'");
    struct arg_lit* no_preserve_root_opt = arg_lit0(NULL, "no-preserve-root", "do not treat '/' specially (the default)");
    struct arg_str* user_opt = arg_str0("u", "user", "USER", "set user USER in the target security context");
    struct arg_str* role_opt = arg_str0("r", "role", "ROLE", "set role ROLE in the target security context");
    struct arg_str* type_opt = arg_str0("t", "type", "TYPE", "set type TYPE in the target security context");
    struct arg_str* range_opt = arg_str0("l", "range", "RANGE", "set range RANGE in the target security context");
    struct arg_str* reference_opt = arg_str0(NULL, "reference", "RFILE", "use RFILE's security context rather than specifying a context");
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE...", 1, 1000, "file(s) to change context of");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {recursive_opt, verbose_opt, no_deref_opt,
                        preserve_root_opt, no_preserve_root_opt,
                        user_opt, role_opt, type_opt, range_opt,
                        reference_opt, help_opt, files_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... CONTEXT FILE...\n", argv[0]);
        printf("  or:  %s [OPTION]... [-u USER] [-r ROLE] [-t TYPE] [-l RANGE] FILE...\n", argv[0]);
        printf("  or:  %s [OPTION]... --reference=RFILE FILE...\n", argv[0]);
        printf("Change the SELinux security context of each FILE to CONTEXT.\n");
        printf("With --reference, change the security context of each FILE to that of RFILE.\n");
        printf("\n");
        printf("  -R, --recursive      operate on files and directories recursively\n");
        printf("  -h, --no-dereference affect symbolic links instead of their target\n");
        printf("  -v, --verbose        output a diagnostic for every file processed\n");
        printf("      --preserve-root  fail to operate recursively on '/'\n");
        printf("      --no-preserve-root  do not treat '/' specially (the default)\n");
        printf("  -u, --user=USER      set user USER in the target security context\n");
        printf("  -r, --role=ROLE      set role ROLE in the target security context\n");
        printf("  -t, --type=TYPE      set type TYPE in the target security context\n");
        printf("  -l, --range=RANGE    set range RANGE in the target security context\n");
        printf("      --reference=RFILE  use RFILE's security context\n");
        printf("      --help           display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    ChconOptions opts;
    opts.is_recursive = (recursive_opt->count > 0);
    opts.is_verbose = (verbose_opt->count > 0);
    opts.no_dereference = (no_deref_opt->count > 0);
    opts.preserve_root = (preserve_root_opt->count > 0);
    opts.user = (user_opt->count > 0) ? user_opt->sval[0] : nullptr;
    opts.role = (role_opt->count > 0) ? role_opt->sval[0] : nullptr;
    opts.type = (type_opt->count > 0) ? type_opt->sval[0] : nullptr;
    opts.range = (range_opt->count > 0) ? range_opt->sval[0] : nullptr;
    opts.reference = (reference_opt->count > 0) ? reference_opt->sval[0] : nullptr;

    // At least one context specification must be given
    bool has_context_spec = (user_opt->count > 0 || role_opt->count > 0 ||
                             type_opt->count > 0 || range_opt->count > 0 ||
                             reference_opt->count > 0);

    if (!has_context_spec) {
        // The first positional arg could be a full context string
        if (files_arg->count < 2) {
            fprintf(stderr, "%s: missing operand\n", argv[0]);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }

        // Treat first positional as full context, rest as files
        // Re-parse: context is files_arg->filename[0], files start at offset 1
        // Actually with arg_file, they're all in files_arg. Let's handle it:
        // If no -u/-r/-t/-l/--reference given, first positional is the full context
        const char* full_ctx = files_arg->filename[0];
        int num_files = files_arg->count - 1;

        chcon_errors = 0;

        for (int i = 0; i < num_files; i++) {
            const char* path = files_arg->filename[i + 1];
            // Set full context on this file
            int set_rc;
            if (opts.no_dereference) {
                set_rc = lsetfilecon(path, full_ctx);
            } else {
                set_rc = setfilecon(path, full_ctx);
            }
            if (set_rc < 0) {
                fprintf(stderr, "chcon: failed to change context of '%s': %s\n",
                        path, strerror(errno));
                chcon_errors = 1;
            } else if (opts.is_verbose) {
                printf("changed context of '%s' to '%s'\n", path, full_ctx);
            }
        }

        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (opts.is_recursive) {
        chcon_glob_opts = &opts;
        for (int i = 0; i < files_arg->count; i++) {
            const char* path = files_arg->filename[i];
            if (nftw(path, recursive_callback, 20, FTW_PHYS) != 0) {
                fprintf(stderr, "chcon: '%s': %s\n", path, strerror(errno));
                chcon_errors = 1;
            }
        }
    } else {
        for (int i = 0; i < files_arg->count; i++) {
            const char* path = files_arg->filename[i];
            if (chcon_one_file(path, &opts) != 0) {
                chcon_errors = 1;
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("chcon", chcon_command, "Change SELinux security context");
