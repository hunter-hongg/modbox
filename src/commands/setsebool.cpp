#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <selinux/selinux.h>

#include "commands/setsebool.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

static int parse_value(const char* s, int* out) {
    if (strcmp(s, "on") == 0 || strcmp(s, "1") == 0 || strcmp(s, "true") == 0) {
        *out = 1;
        return 0;
    }
    if (strcmp(s, "off") == 0 || strcmp(s, "0") == 0 || strcmp(s, "false") == 0) {
        *out = 0;
        return 0;
    }
    return -1;
}

int setsebool_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* persist_opt = arg_lit0("P", "persistent", "set permanent fact in the policy");
    struct arg_file* bools_opt = arg_file0(NULL, NULL, "BOOL...", "booleans to set (name=on|off)");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, persist_opt, bools_opt, end});

    int nerrors = at.parse(argc, argv);
    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... BOOLEAN... on|off\n", argv[0]);
        printf("  or:  %s [OPTION]... BOOLEAN=on|off...\n", argv[0]);
        printf("Set SELinux booleans at runtime (or persist with -P).\n");
        printf("\n");
        printf("      -P, --persistent   set permanent fact in the policy\n");
        printf("      --help             display this help and exit\n");
        printf("      --version          output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("setsebool");
        return 0;
    }

    if (bools_opt->count == 0) {
        fprintf(stderr, "%s: missing operand\n", argv[0]);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    int permanent = persist_opt->count > 0 ? 1 : 0;

    // Detect batch form (name=value tokens) vs legacy form (name value).
    int is_batch = 0;
    for (int i = 0; i < bools_opt->count; i++) {
        if (strchr(bools_opt->filename[i], '=') != nullptr) {
            is_batch = 1;
            break;
        }
    }

    if (is_batch != 0) {
        SELboolean* list = static_cast<SELboolean*>(
            calloc(static_cast<size_t>(bools_opt->count), sizeof(SELboolean)));
        if (list == nullptr) {
            fprintf(stderr, "setsebool: out of memory\n");
            return 1;
        }
        int valid = 0;
        int rc = 0;
        for (int i = 0; i < bools_opt->count; i++) {
            const char* tok = bools_opt->filename[i];
            const char* eq = strchr(tok, '=');
            if (eq == nullptr) {
                fprintf(stderr, "setsebool: invalid boolean assignment '%s'\n", tok);
                rc = 1;
                continue;
            }
            const size_t nlen = static_cast<size_t>(eq - tok);
            char* name = static_cast<char*>(malloc(nlen + 1));
            if (name == nullptr) {
                fprintf(stderr, "setsebool: out of memory\n");
                rc = 1;
                continue;
            }
            memcpy(name, tok, nlen);
            name[nlen] = '\0';
            int value = 0;
            if (parse_value(eq + 1, &value) != 0) {
                fprintf(stderr, "setsebool: invalid value in '%s'\n", tok);
                free(name);
                rc = 1;
                continue;
            }
            list[valid].name = name;
            list[valid].value = value;
            valid++;
        }
        if (valid > 0 && rc == 0) {
            if (security_set_boolean_list(static_cast<size_t>(valid), list, permanent) < 0) {
                fprintf(stderr, "setsebool: unable to set booleans\n");
                rc = 1;
            }
        }
        free(list);
        return rc;
    }

    if (bools_opt->count != 2) {
        fprintf(stderr, "%s: expecting BOOLEAN on|off or BOOLEAN=value (got %d operand(s))\n",
                argv[0], bools_opt->count);
        return 1;
    }

    const char* name = bools_opt->filename[0];
    int value = 0;
    if (parse_value(bools_opt->filename[1], &value) != 0) {
        fprintf(stderr, "setsebool: invalid value '%s' (expected on|off)\n",
                bools_opt->filename[1]);
        return 1;
    }

    // Use the list API so -P persists; the legacy single-name form must honor
    // persistence exactly like the batch form.
    SELboolean single;
    single.name = const_cast<char*>(name);
    single.value = value;
    if (security_set_boolean_list(1, &single, permanent) < 0) {
        fprintf(stderr, "setsebool: unable to set %s to %s\n", name,
                value != 0 ? "on" : "off");
        return 1;
    }
    return 0;
}

REGISTER_COMMAND("setsebool", setsebool_command, "Set SELinux booleans");
