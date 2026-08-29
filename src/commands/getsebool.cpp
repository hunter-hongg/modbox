#include <argtable3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <selinux/selinux.h>

#include "commands/getsebool.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

// Print one boolean's active/pending state. Returns 0 on success, 1 if the
// boolean is unknown or cannot be queried.
static int print_one_boolean(const char* name, const char* prog) {
    int active = security_get_boolean_active(name);
    int pending = security_get_boolean_pending(name);
    if (active < 0 || pending < 0) {
        fprintf(stderr, "%s: %s: no such boolean\n", prog, name);
        return 1;
    }
    if (active == pending) {
        printf("%s --> %s\n", name, active != 0 ? "on" : "off");
    } else {
        printf("%s --> %s %s\n", name,
               active != 0 ? "on" : "off",
               pending != 0 ? "on" : "off");
    }
    return 0;
}

int getsebool_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0(NULL, "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_file* bools_opt = arg_file0(NULL, NULL, "BOOLEAN", "booleans to query");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, bools_opt, end});

    int nerrors = at.parse(argc, argv);
    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [BOOLEAN]...\n", argv[0]);
        printf("Report whether SELinux boolean BOOLEAN is on or off.\n");
        printf("\n");
        printf("      --help           display this help and exit\n");
        printf("      --version        output version information and exit\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("getsebool");
        return 0;
    }

    char** names = nullptr;
    int len = 0;
    if (security_get_boolean_names(&names, &len) < 0) {
        fprintf(stderr, "%s: unable to read SELinux booleans (SELinux disabled or unsupported)\n",
                argv[0]);
        return 1;
    }

    int rc = 0;
    if (bools_opt->count == 0) {
        for (int i = 0; i < len; i++) {
            rc |= print_one_boolean(names[i], argv[0]);
        }
    } else {
        for (int i = 0; i < bools_opt->count; i++) {
            rc |= print_one_boolean(bools_opt->filename[i], argv[0]);
        }
    }

    for (int i = 0; i < len; i++) {
        freecon(names[i]);
    }
    free(static_cast<void*>(names));
    return rc;
}

REGISTER_COMMAND("getsebool", getsebool_command, "Report SELinux boolean states");
