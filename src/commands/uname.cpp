#include <cstdio>
#include <cstring>
#include <sys/utsname.h>
#include <argtable3.h>
#include "commands/uname.hpp"
#include "commands/command_macros.hpp"

void uname_command(int argc, char** argv) {
    struct arg_lit* all_opt = arg_lit0("a", "all", "print all information");
    struct arg_lit* kernel_opt = arg_lit0("s", "kernel-name", "print the kernel name");
    struct arg_lit* node_opt = arg_lit0("n", "nodename", "print the network node hostname");
    struct arg_lit* release_opt = arg_lit0("r", "kernel-release", "print the kernel release");
    struct arg_lit* version_opt = arg_lit0("v", "kernel-version", "print the kernel version");
    struct arg_lit* machine_opt = arg_lit0("m", "machine", "print the machine hardware name");
    struct arg_lit* processor_opt = arg_lit0("p", "processor", "print the processor type (non-portable)");
    struct arg_lit* hardware_opt = arg_lit0("i", "hardware-platform", "print the hardware platform (non-portable)");
    struct arg_lit* os_opt = arg_lit0("o", "operating-system", "print the operating system");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {
        all_opt, kernel_opt, node_opt, release_opt, version_opt,
        machine_opt, processor_opt, hardware_opt, os_opt,
        help_opt, end
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]...\n", argv[0]);
        printf("Print certain system information.\n");
        printf("\n");
        printf("  -a, --all                 print all information\n");
        printf("  -s, --kernel-name         print the kernel name\n");
        printf("  -n, --nodename            print the network node hostname\n");
        printf("  -r, --kernel-release      print the kernel release\n");
        printf("  -v, --kernel-version      print the kernel version\n");
        printf("  -m, --machine             print the machine hardware name\n");
        printf("  -p, --processor           print the processor type (non-portable)\n");
        printf("  -i, --hardware-platform   print the hardware platform (non-portable)\n");
        printf("  -o, --operating-system    print the operating system\n");
        printf("  -h, --help                display this help and exit\n");
        printf("\n");
        printf("If no option is given, -s is implied.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    struct utsname buf;
    if (uname(&buf) < 0) {
        perror("uname");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int any = all_opt->count
           | kernel_opt->count
           | node_opt->count
           | release_opt->count
           | version_opt->count
           | machine_opt->count
           | processor_opt->count
           | hardware_opt->count
           | os_opt->count;

    if (!any) {
        printf("%s\n", buf.sysname);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int first = 1;
#define PRINT_FIELD(field) do { \
    if (!first) putchar(' '); \
    printf("%s", (field)); \
    first = 0; \
} while(0)

    if (all_opt->count || kernel_opt->count)     PRINT_FIELD(buf.sysname);
    if (all_opt->count || node_opt->count)        PRINT_FIELD(buf.nodename);
    if (all_opt->count || release_opt->count)     PRINT_FIELD(buf.release);
    if (all_opt->count || version_opt->count)     PRINT_FIELD(buf.version);
    if (all_opt->count || machine_opt->count)     PRINT_FIELD(buf.machine);

    if (all_opt->count || processor_opt->count) {
        const char* p = buf.machine;
#if defined(__x86_64__) || defined(__i386__)
        p = "x86_64";
#elif defined(__aarch64__)
        p = "aarch64";
#endif
        PRINT_FIELD(p);
    }

    if (all_opt->count || hardware_opt->count) {
        const char* h = buf.machine;
        PRINT_FIELD(h);
    }

    if (all_opt->count || os_opt->count) {
#if defined(__linux__)
        PRINT_FIELD("GNU/Linux");
#elif defined(__APPLE__)
        PRINT_FIELD("Darwin");
#else
        PRINT_FIELD("Unknown");
#endif
    }

#undef PRINT_FIELD
    printf("\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("uname", uname_command, "Print system information");
