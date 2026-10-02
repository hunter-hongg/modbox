// ────── addr2line ──────────────────────────────────────────────────────────
// Convert addresses or symbol+offset into file names and line numbers.
//
// Mirrors GNU addr2line (binutils addr2line.c): addresses are read from the
// command line or stdin, each one is resolved against the executable's
// symbol table and DWARF line info, and file:line[/function] is printed.
//
// Debug info and symbol lookup go through libbfd, so the behaviour (symbol
// matching, "-j" section-relative offsets, "-i" inline unwinding, VMA sign
// extension, address formatting) matches the system addr2line rather than
// approximating it.
// ──────────────────────────────────────────────────────────────────────────

#include <argtable3.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// bfd.h insists on a config.h that only exists inside the binutils build
// tree.  The two macros below satisfy that guard; everything else bfd.h
// needs is already self-contained.
#define PACKAGE 1
#define PACKAGE_VERSION 1
#include <bfd.h>

// DMGL_* come from libiberty's demangle.h, which is not installed by the
// Homebrew binutils formula.  These are stable libiberty ABI values; only
// the bits bfd_demangle() actually inspects are needed here.
static constexpr int DMGL_NO_OPTS          = 0;
static constexpr int DMGL_PARAMS           = (1 << 0);   // include function args
static constexpr int DMGL_ANSI             = (1 << 1);   // ANSI C style
static constexpr int DMGL_JAVA             = (1 << 2);   // demangle as Java
static constexpr int DMGL_AUTO             = (1 << 8);
static constexpr int DMGL_GNU_V3           = (1 << 14);
static constexpr int DMGL_GNAT             = (1 << 15);
static constexpr int DMGL_NO_RECURSE_LIMIT = (1 << 18);

#include "commands/arg_util.hpp"
#include "commands/addr2line.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// ── Options ────────────────────────────────────────────────────────────

struct Addr2LineOptions {
    bool print_address = false;        // -a/--addresses
    bool demangle = false;             // -C/--demangle
    std::string demangle_style;        // --demangle=STYLE
    bool no_recurse_limit = false;     // -r/--no-recurse-limit
    std::string exe_file = "a.out";    // -e/--exe
    bool print_functions = false;      // -f/--functions
    bool show_inlines = false;         // -i/--inlines
    bool pretty_print = false;         // -p/--pretty-print
    bool basename_only = false;        // -s/--basenames
    std::string section_name;          // -j/--section
};

// ── Token grammar ────────────────────────────────────────────────────────

// Split a "symbol+offset" expression, mirroring is_symbol() in binutils
// addr2line.c.  Returns true when the token is a symbol reference; the
// symbol name is written to `sym` (with the "+offset" tail removed) and the
// offset to `offset`.  A bare hex number yields false.
//
// The rules are deliberately literal copies of the original:
//   - leading whitespace is skipped
//   - a digit, or an empty token, is a number
//   - a leading [A-Fa-f] with no '+' anywhere in the token is a number
//   - otherwise the name runs to the next space or '+', and an offset
//     follows only when a '+' comes next (optional whitespace between)
bool is_symbol(const std::string& input, std::string& sym, unsigned long& offset) {
    size_t i = 0;
    while (i < input.size() && isspace((unsigned char)input[i])) {
        i++;
    }
    if (i >= input.size()) {
        return false;  // empty token is a number
    }
    if (isdigit((unsigned char)input[i])) {
        return false;
    }
    {
        char c = (char)toupper((unsigned char)input[i]);
        bool is_hex_letter = (c >= 'A' && c <= 'F');
        if (is_hex_letter && input.find('+', i) == std::string::npos) {
            return false;
        }
    }

    const size_t start = i;
    while (i < input.size() && !isspace((unsigned char)input[i]) && input[i] != '+') {
        i++;
    }
    const size_t end = i;  // NUL position in the original (mutating) version

    while (i < input.size() && isspace((unsigned char)input[i])) {
        i++;
    }

    offset = 0;
    if (i < input.size() && input[i] == '+') {
        i++;
        offset = strtoul(input.c_str() + i, nullptr, 0);
    }

    sym = input.substr(start, end - start);
    return true;
}

// ── Symbol table ─────────────────────────────────────────────────────────

// Loaded once per invocation; the BFD API wants the canonicalized table for
// both symbol lookup and nearest-line queries.
struct SymbolTable {
    asymbol** syms = nullptr;
    long count = 0;

    ~SymbolTable() {
        free(syms);
    }

    bool load(bfd* abfd) {
        long storage = bfd_get_symtab_upper_bound(abfd);
        if (storage < 0) {
            return false;
        }
        if (storage == 0) {
            // No symbols — bfd_canonicalize_symtab would be a no-op.
            count = 0;
            syms = nullptr;
            return true;
        }
        syms = (asymbol**)malloc((size_t)storage);
        if (syms == nullptr) {
            return false;
        }
        count = bfd_canonicalize_symtab(abfd, syms);
        if (count < 0) {
            free(syms);
            syms = nullptr;
            count = 0;
            return false;
        }
        return true;
    }
};

// Find a symbol by name and return value + offset + section VMA, exactly as
// GNU addr2line's lookup_symbol() does.  A second pass compares against the
// demangled names so "Foo::bar(int)" resolves the same as the mangled
// spelling.  Returns 0 when nothing matches.
bfd_vma lookup_symbol(bfd* abfd, const SymbolTable& symtab,
                      const std::string& name, unsigned long offset,
                      int demangle_flags) {
    for (long i = 0; i < symtab.count; i++) {
        const char* symname = symtab.syms[i]->name;
        if (symname != nullptr && strcmp(symname, name.c_str()) == 0) {
            return symtab.syms[i]->value + (bfd_vma)offset +
                   bfd_section_vma(bfd_asymbol_section(symtab.syms[i]));
        }
    }

    // Try again with demangled names.
    for (long i = 0; i < symtab.count; i++) {
        const char* symname = symtab.syms[i]->name;
        if (symname == nullptr || symname[0] == '\0') {
            continue;
        }
        char* demangled = bfd_demangle(abfd, symname, demangle_flags);
        bool match = (demangled != nullptr && strcmp(demangled, name.c_str()) == 0);
        free(demangled);
        if (match) {
            return symtab.syms[i]->value + (bfd_vma)offset +
                   bfd_section_vma(bfd_asymbol_section(symtab.syms[i]));
        }
    }

    return 0;
}

// ── Address → location ──────────────────────────────────────────────────

struct Location {
    const char* filename = nullptr;
    const char* function = nullptr;
    unsigned int line = 0;
    unsigned int discriminator = 0;
    bool found = false;
};

// Callback state used by the section walk; like binutils, the first
// SEC_ALLOC section containing the address wins.
struct FindState {
    bfd_vma pc;
    asymbol** syms;
    Location loc;
};

static void find_address_in_section(bfd* abfd, asection* section, void* data) {
    FindState* state = (FindState*)data;
    if (state->loc.found) {
        return;
    }
    if ((bfd_section_flags(section) & SEC_ALLOC) == 0) {
        return;
    }
    const bfd_vma vma = bfd_section_vma(section);
    if (state->pc < vma) {
        return;
    }
    if (state->pc >= vma + bfd_section_size(section)) {
        return;
    }

    const char* filename = nullptr;
    const char* funcname = nullptr;
    unsigned int line = 0;
    unsigned int discriminator = 0;
    const bool found = bfd_find_nearest_line_discriminator(
        abfd, section, state->syms, state->pc - vma, &filename, &funcname,
        &line, &discriminator);
    if (found) {
        state->loc.found = true;
        state->loc.filename = filename;
        state->loc.function = funcname;
        state->loc.line = line;
        state->loc.discriminator = discriminator;
    }
}

static void find_offset_in_section(bfd* abfd, asection* section, void* data) {
    FindState* state = (FindState*)data;
    if (state->loc.found) {
        return;
    }
    if ((bfd_section_flags(section) & SEC_ALLOC) == 0) {
        return;
    }
    if (state->pc >= (bfd_vma)bfd_section_size(section)) {
        return;
    }

    const char* filename = nullptr;
    const char* funcname = nullptr;
    unsigned int line = 0;
    unsigned int discriminator = 0;
    const bool found = bfd_find_nearest_line_discriminator(
        abfd, section, state->syms, state->pc, &filename, &funcname, &line,
        &discriminator);
    if (found) {
        state->loc.found = true;
        state->loc.filename = filename;
        state->loc.function = funcname;
        state->loc.line = line;
        state->loc.discriminator = discriminator;
    }
}

// ── Output ─────────────────────────────────────────────────────────────

// Print an address with the target's own width, as bfd_printf_vma does.
void print_address(bfd* abfd, bfd_vma value) {
    fputc('0', stdout);
    fputc('x', stdout);
    bfd_printf_vma(abfd, value);
}

// Print "filename:line", applying -s and the "?" / "?"-line fallbacks that
// GNU addr2line uses when the debug info has no answer.
void print_file_line(const Location& loc, const Addr2LineOptions& opts) {
    const char* filename = loc.filename;
    if (opts.basename_only && filename != nullptr) {
        const char* slash = strrchr(filename, '/');
        if (slash != nullptr) {
            filename = slash + 1;
        }
    }
    printf("%s:", filename != nullptr ? filename : "??");
    if (loc.line != 0) {
        if (loc.discriminator != 0) {
            printf("%u (discriminator %u)\n", loc.line, loc.discriminator);
        } else {
            printf("%u\n", loc.line);
        }
    } else {
        printf("?\n");
    }
}

// Emit the function-name portion, demangling when requested.  An empty
// function name means "known location, unnamed function" → "??".
void print_function(bfd* abfd, const Location& loc, bool demangle,
                    int demangle_flags, bool pretty_print) {
    const char* name = loc.function;
    char* allocated = nullptr;

    if (name == nullptr || name[0] == '\0') {
        name = "??";
    } else if (demangle) {
        allocated = bfd_demangle(abfd, name, demangle_flags);
        if (allocated != nullptr) {
            name = allocated;
        }
    }

    fputs(name, stdout);
    if (pretty_print) {
        fputs(" at ", stdout);
    } else {
        fputc('\n', stdout);
    }
    free(allocated);
}

void print_location(bfd* abfd, Location& loc, const Addr2LineOptions& opts,
                    bfd_vma address, int demangle_flags) {
    if (opts.print_address) {
        print_address(abfd, address);
        if (opts.pretty_print) {
            fputs(": ", stdout);
        } else {
            fputc('\n', stdout);
        }
    }

    if (!loc.found) {
        if (opts.print_functions) {
            if (opts.pretty_print) {
                fputs("?? ", stdout);
            } else {
                fputs("??\n", stdout);
            }
        }
        fputs("??:0\n", stdout);
        return;
    }

    // Unwind inline frames when -i was given.
    while (true) {
        if (opts.print_functions) {
            print_function(abfd, loc, opts.demangle, demangle_flags,
                           opts.pretty_print);
        }
        print_file_line(loc, opts);

        if (!opts.show_inlines) {
            break;
        }
        const char* filename = nullptr;
        const char* funcname = nullptr;
        unsigned int line = 0;
        if (!bfd_find_inliner_info(abfd, &filename, &funcname, &line)) {
            break;
        }
        loc.filename = filename;
        loc.function = funcname;
        loc.line = line;
        if (opts.pretty_print) {
            fputs(" (inlined by) ", stdout);
        }
    }
}

// ── Usage / version ─────────────────────────────────────────────────────

void print_usage(const char* prog) {
    printf("Usage: %s [option(s)] [addr(s)]\n", prog);
    printf(" Convert addresses into line number/file name pairs.\n");
    printf(" If no addresses are specified on the command line, they will be read from stdin\n");
    printf(" The options are:\n");
    printf("  -a --addresses         Show addresses\n");
    printf("  -b --target=<bfdname>  Set the binary file format\n");
    printf("  -e --exe=<executable>  Set the input file name (default is a.out)\n");
    printf("  -i --inlines           Unwind inlined functions\n");
    printf("  -j --section=<name>    Read section-relative offsets instead of addresses\n");
    printf("  -p --pretty-print      Make the output easier to read for humans\n");
    printf("  -s --basenames         Strip directory names\n");
    printf("  -f --functions         Show function names\n");
    printf("  -C --demangle[=style]  Demangle function names\n");
    printf("  -R --recurse-limit     Enable a limit on recursion whilst demangling.  [Default]\n");
    printf("  -r --no-recurse-limit  Disable a limit on recursion whilst demangling\n");
    printf("  -h --help              Display this information\n");
    printf("  -v --version           Display the program's version\n");
    printf("\n");
}

// ── Main command ─────────────────────────────────────────────────────────

int run_addr2line(int argc, char** argv) {
    // --demangle=STYLE carries a value that argtable3's flag-only -C cannot
    // consume.  Rewrite those arguments to a bare --demangle first and keep
    // the style separately, so parsing below stays simple.
    std::string demangle_style;
    bool has_demangle_style = false;

    std::vector<char*> argv_copy;
    argv_copy.reserve((size_t)argc);
    for (int i = 0; i < argc; i++) {
        argv_copy.push_back(argv[i]);
    }

    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg == nullptr || arg[0] != '-' || arg[1] != '-') {
            continue;
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
    struct arg_lit* version_alt_opt = arg_lit0("v", nullptr,
        "output version information and exit");
    struct arg_file* addr_arg = arg_filen(nullptr, nullptr, "ADDR", 0, 1000,
        "addresses");
    struct arg_end* end = arg_end(20);

    ArgTable at({addresses_opt, target_opt, demangle_opt, no_recurse_opt,
                 recurse_opt, exe_opt, functions_opt, inlines_opt,
                 pretty_opt, basename_opt, section_opt, help_opt,
                 version_opt, version_alt_opt, addr_arg, end});
    const int nerrors = at.parse(argc, argv_copy.data());

    if (help_opt->count > 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (version_opt->count > 0 || version_alt_opt->count > 0) {
        print_version("addr2line");
        return 0;
    }

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    Addr2LineOptions opts;
    opts.print_address = (addresses_opt->count > 0);
    opts.demangle = (demangle_opt->count > 0);
    opts.print_functions = (functions_opt->count > 0);
    opts.show_inlines = (inlines_opt->count > 0);
    opts.pretty_print = (pretty_opt->count > 0);
    opts.basename_only = (basename_opt->count > 0);
    opts.no_recurse_limit = (no_recurse_opt->count > 0);
    if (has_demangle_style) {
        opts.demangle = true;
        opts.demangle_style = demangle_style;
    }
    if (exe_opt->count > 0 && exe_opt->sval[0] != nullptr) {
        opts.exe_file = exe_opt->sval[0];
    }
    if (section_opt->count > 0 && section_opt->sval[0] != nullptr) {
        opts.section_name = section_opt->sval[0];
    }
    if (target_opt->count > 0 && target_opt->sval[0] != nullptr) {
        // Accepted for compatibility; modbox sniffs the format from the file.
    }

    // Demangling style.  GNU addr2line starts from DMGL_PARAMS | DMGL_ANSI
    // and only the recursion-limit flags adjust it; an explicit style
    // replaces it wholesale.
    int demangle_flags = DMGL_PARAMS | DMGL_ANSI;
    if (opts.demangle && !opts.demangle_style.empty()) {
        if (strcmp(opts.demangle_style.c_str(), "gnu-v3") == 0) {
            demangle_flags = DMGL_PARAMS | DMGL_ANSI | DMGL_GNU_V3;
        } else if (strcmp(opts.demangle_style.c_str(), "java") == 0) {
            demangle_flags = DMGL_PARAMS | DMGL_ANSI | DMGL_JAVA;
        } else if (strcmp(opts.demangle_style.c_str(), "auto") == 0) {
            demangle_flags = DMGL_PARAMS | DMGL_ANSI | DMGL_AUTO;
        } else if (strcmp(opts.demangle_style.c_str(), "gnat") == 0) {
            demangle_flags = DMGL_PARAMS | DMGL_ANSI | DMGL_GNAT;
        } else {
            fprintf(stderr, "addr2line: unknown demangling style '%s'\n",
                    opts.demangle_style.c_str());
            return 1;
        }
    }
    if (opts.no_recurse_limit) {
        demangle_flags |= DMGL_NO_RECURSE_LIMIT;
    } else {
        demangle_flags &= ~DMGL_NO_RECURSE_LIMIT;
    }

    // Open the executable.
    bfd_init();

    bfd* abfd = bfd_openr(opts.exe_file.c_str(), nullptr);
    if (abfd == nullptr) {
        fprintf(stderr, "addr2line: %s: %s\n", opts.exe_file.c_str(),
                bfd_errmsg(bfd_get_error()));
        return 1;
    }

    char** matching = nullptr;
    if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
        // Not an object file (e.g. a text file was passed to -e).  The
        // detailed reason is only meaningful for ambiguous matches, so fall
        // back to the generic wording the reference uses.
        const char* err = matching != nullptr ? bfd_errmsg(bfd_get_error())
                                              : "File format not recognized";
        fprintf(stderr, "addr2line: %s: %s\n", opts.exe_file.c_str(), err);
        bfd_close(abfd);
        return 1;
    }

    SymbolTable symtab;
    if (!symtab.load(abfd)) {
        fprintf(stderr, "addr2line: %s: %s\n", opts.exe_file.c_str(),
                bfd_errmsg(bfd_get_error()));
        bfd_close(abfd);
        return 1;
    }

    // -j selects a single section and makes the input offsets
    // section-relative instead of absolute addresses.
    asection* section = nullptr;
    if (!opts.section_name.empty()) {
        section = bfd_get_section_by_name(abfd, opts.section_name.c_str());
        if (section == nullptr) {
            fprintf(stderr, "addr2line: %s: cannot find section '%s'\n",
                    opts.exe_file.c_str(), opts.section_name.c_str());
            bfd_close(abfd);
            return 1;
        }
    }

    // Collect the addresses: from the command line, otherwise from stdin.
    std::vector<std::string> addresses;
    if (addr_arg->count > 0) {
        for (int i = 0; i < addr_arg->count; i++) {
            const char* addr = addr_arg->filename[i];
            if (addr != nullptr) {
                addresses.push_back(addr);
            }
        }
    } else {
        // Read from stdin, one address per line.  A line containing only
        // whitespace is still a token — GNU addr2line's fgets loop hands
        // every non-failed read to is_symbol(), and the empty string there
        // parses as the number 0.
        char buf[256];
        while (fgets(buf, sizeof(buf), stdin) != nullptr) {
            size_t len = strlen(buf);
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
                buf[--len] = '\0';
            }
            addresses.push_back(buf);
        }
    }

    // ELF targets mask the address to the architecture width and, on some,
    // sign-extend it.  Match that so the same hex input resolves the same
    // way the system addr2line would resolve it.
    const bool is_elf = (bfd_get_flavour(abfd) == bfd_target_elf_flavour);
    const int arch_size = is_elf ? bfd_get_arch_size(abfd) : 0;
    const bool sign_extend = is_elf && (bfd_get_sign_extend_vma(abfd) != 0);

    for (const std::string& input : addresses) {
        std::string sym_name;
        unsigned long offset = 0;
        const bool is_sym = is_symbol(input, sym_name, offset);

        bfd_vma pc;
        if (is_sym) {
            pc = lookup_symbol(abfd, symtab, sym_name, offset, demangle_flags);
        } else {
            // strtoull with base 16 reproduces bfd_scan_vma's own parsing:
            // it accepts an optional leading '+' and a 0x/0 prefix.
            errno = 0;
            char* endptr = nullptr;
            pc = (bfd_vma)strtoull(input.c_str(), &endptr, 16);
            (void)endptr;
        }

        if (is_elf && arch_size > 0) {
            const bfd_vma sign = (bfd_vma)1 << (arch_size - 1);
            pc &= (sign << 1) - 1;
            if (sign_extend) {
                pc = (pc ^ sign) - sign;
            }
        }

        FindState state;
        state.pc = pc;
        state.syms = symtab.syms;

        if (section != nullptr) {
            find_offset_in_section(abfd, section, &state);
        } else {
            bfd_map_over_sections(abfd, find_address_in_section, &state);
        }

        print_location(abfd, state.loc, opts, pc, demangle_flags);
        fflush(stdout);
    }

    bfd_close(abfd);
    return 0;
}

}  // namespace

int addr2line_command(int argc, char** argv) {
    return run_addr2line(argc, argv);
}

REGISTER_COMMAND("addr2line", addr2line_command, "Convert addresses to file names and line numbers");
