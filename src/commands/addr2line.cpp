// ────── addr2line ──────────────────────────────────────────────────────────
// Convert addresses or symbol+offset into file names and line numbers.
// Uses libelf + libdw (elfutils) for DWARF parsing and libbfd for
// demangling. Matches GNU addr2line 2.46 output format.
// ──────────────────────────────────────────────────────────────────────────

#include <argtable3.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// libelf / libdw from elfutils
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <gelf.h>

#include "commands/arg_util.hpp"
#include "commands/addr2line.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

// ── bfd demangle (from libbfd, via Homebrew binutils) ──────────────────
// We declare bfd_demangle ourselves because bfd.h requires config.h which
// is internal to the binutils build tree. The DMGL_* constants are from
// libiberty's demangle.h and are stable ABI values.
extern "C" {
char* bfd_demangle(void* abfd, const char* name, int flags);
}

// DMGL flag values (from libiberty's demangle.h).
// These bit positions are defined by libiberty and must not be changed.
static constexpr int DMGL_NO_OPTS  = 0;
static constexpr int DMGL_PARAMS   = (1 << 0);
static constexpr int DMGL_VERBOSE  = (1 << 1);
static constexpr int DMGL_TYPES    = (1 << 2);
static constexpr int DMGL_RET_POST = (1 << 3);
static constexpr int DMGL_AUTO     = (1 << 4);
static constexpr int DMGL_FILE     = (1 << 5);
static constexpr int DMGL_VIEW     = (1 << 6);
static constexpr int DMGL_GNU      = (1 << 7);
static constexpr int DMGL_LUCID    = (1 << 8);
static constexpr int DMGL_ARM      = (1 << 9);
static constexpr int DMGL_JAVA     = (1 << 10);
static constexpr int DMGL_ACT      = (1 << 11);
static constexpr int DMGL_IBM      = (1 << 12);
static constexpr int DMGL_GNU_V3   = (1 << 13);
static constexpr int DMGL_HP       = (1 << 14);
static constexpr int DMGL_REMOTE   = (1 << 15);

namespace {

// ── Options ────────────────────────────────────────────────────────────

struct Addr2LineOptions {
    bool print_address = false;        // -a/--addresses
    bool demangle = false;             // -C/--demangle
    std::string demangle_style;        // --demangle=STYLE
    bool recurse_limit = true;         // -R/--recurse-limit (default on)
    std::string exe_file = "a.out";    // -e/--exe
    bool print_functions = false;      // -f/--functions
    bool show_inlines = false;         // -i/--inlines
    bool pretty_print = false;         // -p/--pretty-print
    bool basename_only = false;         // -s/--basenames
};

// ── Demangling ───────────────────────────────────────────────────────────

// Convert a demangle style name to DMGL style flags.
// Returns -1 for unknown style names.
int demangle_style_to_flags(const char* style) {
    if (style == nullptr || *style == '\0') {
        return DMGL_AUTO;  // default: autodetect
    }

    if (strcmp(style, "gnu") == 0 || strcmp(style, "g") == 0 ||
        strcmp(style, "gnulib") == 0) {
        return DMGL_GNU;
    }
    if (strcmp(style, "lucid") == 0 || strcmp(style, "l") == 0) {
        return DMGL_LUCID;
    }
    if (strcmp(style, "arm") == 0) {
        return DMGL_ARM;
    }
    if (strcmp(style, "java") == 0 || strcmp(style, "j") == 0) {
        return DMGL_JAVA;
    }
    if (strcmp(style, "auto") == 0) {
        return DMGL_AUTO;
    }
    return -1;  // unknown style
}

// ── DWFL callbacks ───────────────────────────────────────────────────────

// For offline (file-based) use, find_elf is not needed since
// dwfl_report_offline already provides the ELF data.
static int find_elf_mod(Dwfl_Module*, void**, const char*, Dwarf_Addr,
                        char**, Elf**) {
    return -1;
}

static const Dwfl_Callbacks dwfl_callbacks = {
    .find_elf = find_elf_mod,
    .find_debuginfo = nullptr,
    .debuginfo_path = nullptr,
};

// ── Address resolution ───────────────────────────────────────────────────

struct ResolvedLocation {
    std::string function;
    std::string filename;
    int line;
    bool has_location;   // true if file:line was resolved from DWARF
    bool has_symbol;     // true if a symbol/function name was found
};

// Resolve an address to file:line and optionally function name.
// The `bias` is the module load bias from dwfl_module_getelf; the
// user-supplied address is a raw ELF virtual address without the bias.
ResolvedLocation resolve_address(Dwfl_Module* mod, Dwarf_Addr address,
                                 Dwarf_Addr bias) {
    ResolvedLocation result = {"??", "??", 0, false, false};

    if (!mod) {
        return result;
    }

    // Get source line info — dwfl_module_getsrc expects the address
    // adjusted by the module's load bias.
    Dwarf_Addr adjusted = address + bias;
    Dwfl_Line* line = dwfl_module_getsrc(mod, adjusted);
    if (line) {
        Dwarf_Addr addr_out;
        int line_out = 0;
        int col_out = 0;
        Dwarf_Word mtime = 0;
        Dwarf_Word length = 0;
        const char* file = dwfl_lineinfo(line, &addr_out, &line_out, &col_out,
                                        &mtime, &length);
        if (file) {
            result.filename = file;
            result.line = line_out;
            result.has_location = true;
        }
    }

    // Get function name
    GElf_Sym sym;
    GElf_Word shndx;
    const char* symname = dwfl_module_addrsym(mod, adjusted, &sym, &shndx);
    if (symname) {
        result.function = symname;
        result.has_symbol = true;
    }

    return result;
}

// Resolve a symbol+offset expression by looking up the symbol in the
// symbol table and adding the offset to its value.
bool resolve_symbol_offset(Dwfl_Module* mod, const std::string& input,
                           Dwarf_Addr& out_addr) {
    size_t plus_pos = input.find_last_of('+');
    if (plus_pos == std::string::npos || plus_pos == 0) {
        return false;
    }

    std::string sym_name = input.substr(0, plus_pos);
    std::string offset_str = input.substr(plus_pos + 1);

    char* end = nullptr;
    errno = 0;
    unsigned long offset = strtoul(offset_str.c_str(), &end, 0);
    if (errno != 0 || end == offset_str.c_str() || *end != '\0') {
        return false;
    }

    int symcount = dwfl_module_getsymtab(mod);
    if (symcount < 0) {
        return false;
    }

    bool found = false;
    for (int i = 0; i < symcount; i++) {
        GElf_Sym sym;
        GElf_Word shndx;
        const char* name = dwfl_module_getsym(mod, i, &sym, &shndx);
        if (name && strcmp(name, sym_name.c_str()) == 0) {
            out_addr = sym.st_value + (Dwarf_Addr)offset;
            found = true;
            break;
        }
    }

    return found;
}

// ── Output helpers ───────────────────────────────────────────────────────

void print_filename(const std::string& filename, bool basename_only) {
    if (basename_only) {
        size_t pos = filename.find_last_of('/');
        if (pos != std::string::npos) {
            fputs(filename.c_str() + pos + 1, stdout);
        } else {
            fputs(filename.c_str(), stdout);
        }
    } else {
        fputs(filename.c_str(), stdout);
    }
}

// Print the file:line portion. When debug info is absent for the address:
// - if a symbol was found, the line is "?" (unknown line in known function)
// - if no symbol was found, the line is "0" (completely unknown address)
void print_file_line(const std::string& filename, int line, bool has_location,
                     bool has_symbol, bool basename_only) {
    print_filename(filename, basename_only);
    if (has_location) {
        printf(":%d\n", line);
    } else if (has_symbol) {
        printf(":?\n");
    } else {
        printf(":0\n");
    }
}

void print_location(const ResolvedLocation& loc, const Addr2LineOptions& opts,
                    Dwarf_Addr address) {
    if (opts.pretty_print) {
        // Pretty-print: single line.
        // Format with -a: "0xADDR: [FUNC at ]FILE:LINE"
        // Format without -a: "[FUNC at ]FILE:LINE"
        // When function is "??": "?? ??:0" instead of "?? at ??:0"
        if (opts.print_address) {
            printf("0x%016lx:", (unsigned long)address);
        }
        if (opts.print_functions && loc.has_symbol) {
            if (opts.print_address) {
                printf(" ");
            }
            if (loc.function != "??") {
                printf("%s at ", loc.function.c_str());
            } else {
                printf("?? ");
            }
            print_file_line(loc.filename, loc.line, loc.has_location,
                            loc.has_symbol, opts.basename_only);
        } else if (opts.print_functions && !loc.has_symbol) {
            // Unknown function — system addr2line prints "?? ??:0"
            if (opts.print_address) {
                printf(" ");
            }
            printf("?? ");
            print_file_line(loc.filename, loc.line, loc.has_location,
                            opts.basename_only);
        } else {
            // No -f: just print file:line
            if (opts.print_address) {
                printf(" ");
            }
            print_file_line(loc.filename, loc.line, loc.has_location,
                            opts.basename_only);
        }
    } else {
        // Normal (non-pretty) output
        if (opts.print_address) {
            printf("0x%016lx\n", (unsigned long)address);
        }
        if (opts.print_functions) {
            if (loc.has_symbol) {
                printf("%s\n", loc.function.c_str());
            } else {
                printf("??\n");
            }
        }
        print_file_line(loc.filename, loc.line, loc.has_location,
                        opts.basename_only);
    }
}

// ── Main command ─────────────────────────────────────────────────────────

void print_usage(const char* prog) {
    printf("Usage: %s [OPTION(S)] [addr addr ...]\n", prog);
    printf("Convert addresses to file names and line numbers.\n");
    printf("\n");
    printf("  -a, --addresses        print the address before the function\n");
    printf("                         name, file name and line number\n");
    printf("  -b, --target=BFDNAME   target object-code format\n");
    printf("  -C, --demangle[=STYLE] print demangled symbol names\n");
    printf("  -e, --exe=FILENAME     specify the executable to be read\n");
    printf("  -f, --functions        also display the function name\n");
    printf("  -i, --inlines          print enclosing/inlined functions\n");
    printf("  -p, --pretty-print     human-friendly output\n");
    printf("  -s, --basenames        strip directory from file names\n");
    printf("  -j, --section=NAME     specify the section\n");
    printf("  -r, --no-recurse-limit disable recursion limit\n");
    printf("  -R, --recurse-limit    enable recursion limit (default)\n");
    printf("  -h, --help             display this help and exit\n");
    printf("  -V, --version          output version information and exit\n");
}

int run_addr2line(int argc, char** argv) {
    // Pre-process argv to extract --demangle=STYLE from the long option.
    // argtable3 treats -C as a flag (arg_lit0) — it does not consume the
    // next argument. But --demangle=STYLE needs a value. We intercept
    // --demangle=STYLE here and rewrite it to --demangle before argtable3
    // sees it, capturing the style separately.
    std::string demangle_style;
    bool has_demangle_style = false;

    std::vector<char*> argv_copy;
    for (int i = 0; i < argc; i++) {
        argv_copy.push_back(argv[i]);
    }

    for (int i = 1; i < argc; i++) {
        char* arg = argv_copy[i];
        if (arg == nullptr || arg[0] != '-' || arg[1] != '-') {
            continue;
        }
        if (strcmp(arg, "--demangle") == 0) {
            continue;  // flag, handled by arg_lit0
        }
        if (strncmp(arg, "--demangle=", 11) == 0) {
            demangle_style = arg + 11;
            has_demangle_style = true;
            argv_copy[i] = (char*)"--demangle";
        }
    }

    struct arg_lit* addresses_opt = arg_lit0("a", "addresses",
        "print the address before the location info");
    struct arg_str* target_opt = arg_str0("b", "target", "BFDNAME",
        "target object-code format");
    struct arg_lit* demangle_opt = arg_lit0("C", "demangle",
        "demangle symbol names");
    struct arg_lit* no_recurse_opt = arg_lit0("r", "no-recurse-limit",
        "disable demangling recursion limit");
    struct arg_lit* recurse_opt = arg_lit0("R", "recurse-limit",
        "enable demangling recursion limit");
    struct arg_str* exe_opt = arg_str0("e", "exe", "FILENAME",
        "specify the executable");
    struct arg_lit* functions_opt = arg_lit0("f", "functions",
        "also display function names");
    struct arg_lit* inlines_opt = arg_lit0("i", "inlines",
        "print inlined functions");
    struct arg_lit* pretty_opt = arg_lit0("p", "pretty-print",
        "human-friendly output");
    struct arg_lit* basename_opt = arg_lit0("s", "basenames",
        "strip directory from file names");
    struct arg_str* section_opt = arg_str0("j", "section", "NAME",
        "specify the section");
    struct arg_lit* help_opt = arg_lit0("h", "help",
        "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version",
        "output version information and exit");
    struct arg_file* addr_arg = arg_filen(nullptr, nullptr, "ADDR", 0, 1000,
        "addresses");
    struct arg_end* end = arg_end(20);

    ArgTable at({addresses_opt, target_opt, demangle_opt, no_recurse_opt,
                 recurse_opt, exe_opt, functions_opt, inlines_opt,
                 pretty_opt, basename_opt, section_opt, help_opt,
                 version_opt, addr_arg, end});
    const int nerrors = at.parse(argc, argv_copy.data());

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("addr2line");
        return 0;
    }

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    // Build options
    Addr2LineOptions opts;
    opts.print_address = (addresses_opt->count > 0);
    opts.demangle = (demangle_opt->count > 0);
    opts.print_functions = (functions_opt->count > 0);
    opts.show_inlines = (inlines_opt->count > 0);
    opts.pretty_print = (pretty_opt->count > 0);
    opts.basename_only = (basename_opt->count > 0);
    opts.recurse_limit = !(no_recurse_opt->count > 0);

    if (has_demangle_style) {
        opts.demangle = true;
        opts.demangle_style = demangle_style;
    }

    // Build demangle flags — DMGL_AUTO with DMGL_PARAMS to show parameters,
    // matching GNU addr2line/c++filt defaults.
    int demangle_flags = DMGL_NO_OPTS;
    if (opts.demangle) {
        if (!opts.demangle_style.empty()) {
            int flags = demangle_style_to_flags(opts.demangle_style.c_str());
            if (flags < 0) {
                fprintf(stderr, "addr2line: unknown demangling style '%s'\n",
                        opts.demangle_style.c_str());
                demangle_flags = DMGL_NO_OPTS;
            } else {
                demangle_flags = flags | DMGL_PARAMS;
            }
        } else {
            demangle_flags = DMGL_AUTO | DMGL_PARAMS;
        }
    }

    // Executable file
    if (exe_opt->count > 0 && exe_opt->sval[0]) {
        opts.exe_file = exe_opt->sval[0];
    }

    // Initialize DWFL
    Dwfl* dwfl = dwfl_begin(&dwfl_callbacks);
    if (!dwfl) {
        fprintf(stderr, "addr2line: dwfl_begin failed\n");
        return 1;
    }

    // Open the executable and report it as an offline module.
    // Using the fd form ensures the ELF is properly loaded.
    int fd = open(opts.exe_file.c_str(), O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "addr2line: cannot open %s: %s\n",
                opts.exe_file.c_str(), strerror(errno));
        dwfl_end(dwfl);
        return 1;
    }

    Dwfl_Module* mod = dwfl_report_offline(dwfl, "modbox-addr2line",
                                           opts.exe_file.c_str(), fd);
    close(fd);
    if (!mod) {
        fprintf(stderr, "addr2line: cannot read debug info from %s: %s\n",
                opts.exe_file.c_str(), dwfl_errmsg(-1));
        dwfl_end(dwfl);
        return 1;
    }

    // Get the module bias. For PIE executables, the ELF virtual addresses
    // differ from what dwfl_module_getsrc expects; the bias must be added
    // to user-supplied (nm-style) addresses.
    GElf_Addr mod_bias = 0;
    if (dwfl_module_getelf(mod, &mod_bias) == nullptr) {
        fprintf(stderr, "addr2line: dwfl_module_getelf failed: %s\n",
                dwfl_errmsg(-1));
        dwfl_end(dwfl);
        return 1;
    }

    // Collect addresses: from command line args or stdin
    std::vector<std::string> addresses;

    if (addr_arg->count > 0) {
        for (int i = 0; i < addr_arg->count; i++) {
            const char* addr = addr_arg->filename[i];
            if (addr) {
                addresses.push_back(addr);
            }
        }
    } else {
        // Read from stdin, one address per line
        char buf[256];
        while (fgets(buf, sizeof(buf), stdin) != nullptr) {
            char* end = buf + strlen(buf) - 1;
            while (end > buf && (*end == '\n' || *end == '\r' ||
                                 *end == ' ' || *end == '\t')) {
                *end-- = '\0';
            }
            if (buf[0] != '\0') {
                addresses.push_back(buf);
            }
        }
    }

    // Process each address
    int status = 0;
    for (const std::string& input : addresses) {
        Dwarf_Addr address = 0;
        bool parsed = false;

        // Try to parse as a number (hex or decimal)
        char* endptr = nullptr;
        errno = 0;
        address = (Dwarf_Addr)strtoull(input.c_str(), &endptr, 0);
        if (errno == 0 && endptr != input.c_str() && *endptr == '\0') {
            parsed = true;
        } else {
            // Try symbol+offset format (e.g. "main+0x10")
            if (resolve_symbol_offset(mod, input, address)) {
                parsed = true;
            } else {
                // Completely unparseable input — treat as address 0
                address = 0;
                parsed = true;
            }
        }

        if (!parsed) {
            continue;
        }

        // Resolve the address
        ResolvedLocation loc = resolve_address(mod, address, mod_bias);

        // Demangle the function name if requested
        if (opts.demangle && loc.has_symbol && loc.function != "??") {
            char* demangled = bfd_demangle(nullptr, loc.function.c_str(),
                                           demangle_flags);
            if (demangled) {
                loc.function = demangled;
                free(demangled);
            }
        }

        // Print output
        print_location(loc, opts, address);
    }

    dwfl_end(dwfl);
    return status;
}

}  // namespace

int addr2line_command(int argc, char** argv) {
    return run_addr2line(argc, argv);
}

REGISTER_COMMAND("addr2line", addr2line_command, "Convert addresses to file names and line numbers");
