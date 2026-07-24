#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>

#include "commands/dircolors.hpp"
#include "commands/command_macros.hpp"

static const char* default_database =
    "# Default LS_COLORS database\n"
    "RESET 0\n"
    "DIR 01;34\n"
    "LINK 01;36\n"
    "SYMLINK 01;36\n"
    "FIFO 40;33\n"
    "SOCK 01;35\n"
    "DOOR 01;35\n"
    "BLK 40;33;01\n"
    "CHR 40;33;01\n"
    "EXEC 01;32\n"
    "SUID 37;41\n"
    "SGID 30;43\n"
    "STICKY 37;44\n"
    "STICKY_OTHER_WRITABLE 30;42\n"
    "OTHER_WRITABLE 34;42\n"
    "ORPHAN 40;31;01\n"
    "MISSING 00\n"
    ".tar 01;31\n"
    ".tgz 01;31\n"
    ".arc 01;31\n"
    ".arj 01;31\n"
    ".taz 01;31\n"
    ".lha 01;31\n"
    ".lz4 01;31\n"
    ".lzh 01;31\n"
    ".lzma 01;31\n"
    ".tlz 01;31\n"
    ".txz 01;31\n"
    ".tzo 01;31\n"
    ".t7z 01;31\n"
    ".zip 01;31\n"
    ".z 01;31\n"
    ".dz 01;31\n"
    ".gz 01;31\n"
    ".lrz 01;31\n"
    ".lz 01;31\n"
    ".lzo 01;31\n"
    ".xz 01;31\n"
    ".zst 01;31\n"
    ".tzst 01;31\n"
    ".bz2 01;31\n"
    ".bz 01;31\n"
    ".tbz 01;31\n"
    ".tbz2 01;31\n"
    ".tz 01;31\n"
    ".deb 01;31\n"
    ".rpm 01;31\n"
    ".jar 01;31\n"
    ".war 01;31\n"
    ".ear 01;31\n"
    ".sar 01;31\n"
    ".xpi 01;31\n"
    ".apk 01;31\n"
    ".a 01;31\n"
    ".lib 01;31\n"
    ".so 01;31\n"
    ".so.0 01;31\n"
    ".so.1 01;31\n"
    ".so.2 01;31\n"
    ".so.3 01;31\n"
    ".so.4 01;31\n"
    ".so.5 01;31\n"
    ".so.6 01;31\n"
    ".so.7 01;31\n"
    ".so.8 01;31\n"
    ".so.9 01;31\n"
    ".so.10 01;31\n"
    ".so.11 01;31\n"
    ".so.12 01;31\n"
    ".so.13 01;31\n"
    ".so.14 01;31\n"
    ".so.15 01;31\n"
    ".so.16 01;31\n"
    ".so.17 01;31\n"
    ".so.18 01;31\n"
    ".so.19 01;31\n"
    ".so.20 01;31\n"
    ".so.21 01;31\n"
    ".so.22 01;31\n"
    ".so.23 01;31\n"
    ".so.24 01;31\n"
    ".so.25 01;31\n"
    ".so.26 01;31\n"
    ".so.27 01;31\n"
    ".so.28 01;31\n"
    ".so.29 01;31\n"
    ".so.30 01;31\n"
    ".so.31 01;31\n"
    ".so.32 01;31\n"
    ".so.33 01;31\n"
    ".so.34 01;31\n"
    ".so.35 01;31\n"
    ".so.36 01;31\n"
    ".so.37 01;31\n"
    ".so.38 01;31\n"
    ".so.39 01;31\n"
    ".so.40 01;31\n"
    ".so.41 01;31\n"
    ".so.42 01;31\n"
    ".so.43 01;31\n"
    ".so.44 01;31\n"
    ".so.45 01;31\n"
    ".so.46 01;31\n"
    ".so.47 01;31\n"
    ".so.48 01;31\n"
    ".so.49 01;31\n"
    ".so.50 01;31\n"
    ".so.51 01;31\n"
    ".so.52 01;31\n"
    ".so.53 01;31\n"
    ".so.54 01;31\n"
    ".so.55 01;31\n"
    ".so.56 01;31\n"
    ".so.57 01;31\n"
    ".so.58 01;31\n"
    ".so.59 01;31\n"
    ".so.60 01;31\n"
    ".so.61 01;31\n"
    ".so.62 01;31\n"
    ".so.63 01;31\n"
    ".so.64 01;31\n"
    ".so.65 01;31\n"
    ".so.66 01;31\n"
    ".so.67 01;31\n"
    ".so.68 01;31\n"
    ".so.69 01;31\n"
    ".so.70 01;31\n"
    ".so.71 01;31\n"
    ".so.72 01;31\n"
    ".so.73 01;31\n"
    ".so.74 01;31\n"
    ".so.75 01;31\n"
    ".so.76 01;31\n"
    ".so.77 01;31\n"
    ".so.78 01;31\n"
    ".so.79 01;31\n"
    ".so.80 01;31\n"
    ".so.81 01;31\n"
    ".so.82 01;31\n"
    ".so.83 01;31\n"
    ".so.84 01;31\n"
    ".so.85 01;31\n"
    ".so.86 01;31\n"
    ".so.87 01;31\n"
    ".so.88 01;31\n"
    ".so.89 01;31\n"
    ".so.90 01;31\n"
    ".so.91 01;31\n"
    ".so.92 01;31\n"
    ".so.93 01;31\n"
    ".so.94 01;31\n"
    ".so.95 01;31\n"
    ".so.96 01;31\n"
    ".so.97 01;31\n"
    ".so.98 01;31\n"
    ".so.99 01;31\n"
    ".so.100 01;31\n"
    ".o 01;31\n"
    ".obj 01;31\n"
    ".mod 01;31\n"
    ".rbc 01;31\n"
    ".go 00;32\n"
    ".rs 00;32\n"
    ".py 00;32\n"
    ".rb 00;32\n"
    ".js 00;32\n"
    ".ts 00;32\n"
    ".c 00;32\n"
    ".h 00;32\n"
    ".cpp 00;32\n"
    ".hpp 00;32\n"
    ".cxx 00;32\n"
    ".cc 00;32\n"
    ".hxx 00;32\n"
    ".hh 00;32\n"
    ".o 01;31\n"
    ".a 01;31\n"
    ".so 01;31\n"
    ".dylib 01;31\n"
    ".lib 01;31\n"
    ".dll 01;31\n"
    ".cr 00;32\n"
    ".java 00;32\n"
    ".class 01;31\n"
    ".jar 01;31\n"
    ".war 01;31\n"
    ".ear 01;31\n"
    ".xpi 01;31\n"
    ".apk 01;31\n"
    ".gemspec 01;31\n"
    ".pem 00;35\n"
    ".p12 00;35\n"
    ".crt 00;35\n"
    ".sst 00;35\n"
    ".crt 00;35\n"
    ".key 00;35\n"
    ".pub 00;35\n"
    ".pfx 00;35\n"
    ".p8 00;35\n"
    ".pem 00;35\n"
    ".sig 00;35\n"
    ".gpg 00;35\n"
    ".pgp 00;35\n"
    ".ssh 00;35\n"
    ".asc 00;35\n"
    ".md 00;36\n"
    ".markdown 00;36\n"
    ".rst 00;36\n"
    ".txt 00;36\n"
    ".text 00;36\n"
    ".gitignore 00;36\n"
    ".cfg 00;36\n"
    ".conf 00;36\n"
    ".ini 00;36\n"
    ".log 00;36\n"
    ".yaml 00;36\n"
    ".yml 00;36\n"
    ".json 00;36\n"
    ".toml 00;36\n"
    ".xml 00;36\n"
    ".html 00;36\n"
    ".htm 00;36\n"
    ".css 00;36\n"
    ".scss 00;36\n"
    ".less 00;36\n"
    ".sql 00;36\n"
    ".sh 00;36\n"
    ".bash 00;36\n"
    ".zsh 00;36\n"
    ".fish 00;36\n"
    ".csh 00;36\n"
    ".ksh 00;36\n"
    ".bat 00;36\n"
    ".cmd 00;36\n"
    ".ps1 00;36\n"
    ".vbs 00;36\n"
    ".vbe 00;36\n"
    ".js 00;36\n"
    ".mjs 00;36\n"
    ".cjs 00;36\n"
    ".ts 00;36\n"
    ".tsx 00;36\n"
    ".jsx 00;36\n"
    ".mts 00;36\n"
    ".cts 00;36\n"
    ".vue 00;36\n"
    ".svelte 00;36\n"
    ".elm 00;36\n"
    ".ex 00;36\n"
    ".exs 00;36\n"
    ".eex 00;36\n"
    ".erl 00;36\n"
    ".hrl 00;36\n"
    ".xrl 00;36\n"
    ".yrl 00;36\n"
    ".leex 00;36\n"
    ".erl 00;36\n"
    ".hrl 00;36\n"
    ".app 01;31\n"
    ".ipa 01;31\n"
    ".dmg 01;31\n"
    ".iso 01;31\n"
    ".img 01;31\n"
    ".msi 01;31\n"
    ".msp 01;31\n"
    ".msm 01;31\n"
    ".rpm 01;31\n"
    ".deb 01;31\n"
    ".pkg 01;31\n"
    ".snap 01;31\n"
    ".flatpak 01;31\n"
    ".appimage 01;31\n"
    ".pdf 01;33\n"
    ".dvi 01;33\n"
    ".ps 01;33\n"
    ".eps 01;33\n"
    ".fig 01;33\n"
    ".xcf 01;33\n"
    ".ai 01;33\n"
    ".svg 01;33\n"
    ".png 01;33\n"
    ".jpg 01;33\n"
    ".jpeg 01;33\n"
    ".gif 01;33\n"
    ".bmp 01;33\n"
    ".tif 01;33\n"
    ".tiff 01;33\n"
    ".ico 01;33\n"
    ".heic 01;33\n"
    ".heif 01;33\n"
    ".raw 01;33\n"
    ".webp 01;33\n"
    ".3gp 01;33\n"
    ".avi 01;33\n"
    ".flv 01;33\n"
    ".mkv 01;33\n"
    ".mov 01;33\n"
    ".mp4 01;33\n"
    ".mpeg 01;33\n"
    ".mpg 01;33\n"
    ".ogv 01;33\n"
    ".webm 01;33\n"
    ".m4v 01;33\n"
    ".vob 01;33\n"
    ".wmv 01;33\n"
    ".asf 01;33\n"
    ".rm 01;33\n"
    ".rmvb 01;33\n"
    ".yuv 01;33\n"
    ".dv 01;33\n"
    ".m4a 01;33\n"
    ".m4b 01;33\n"
    ".m4p 01;33\n"
    ".m4r 01;33\n"
    ".aac 01;33\n"
    ".au 01;33\n"
    ".flac 01;33\n"
    ".mid 01;33\n"
    ".midi 01;33\n"
    ".mka 01;33\n"
    ".mp3 01;33\n"
    ".mpc 01;33\n"
    ".ogg 01;33\n"
    ".opus 01;33\n"
    ".ra 01;33\n"
    ".spx 01;33\n"
    ".wav 01;33\n"
    ".wma 01;33\n"
    ".wv 01;33\n"
    ".oga 01;33\n"
    ".spx 01;33\n"
    ".xspf 01;33\n"
    ".pls 01;33\n"
    ".m3u 01;33\n"
    ".m3u8 01;33\n"
    ".wpl 01;33\n"
    ".pl 01;35\n"
    ".pm 01;35\n"
    ".pod 01;35\n"
    ".t 01;35\n"
    ".rhtml 01;35\n"
    ".tt2 01;35\n"
    ".ttml 01;35\n"
    ".cr 01;35\n"
    ".purs 01;35\n"
    ".r 01;35\n"
    ".0 40;33;01\n"
    ".1 40;33;01\n"
    ".2 40;33;01\n"
    ".3 40;33;01\n"
    ".4 40;33;01\n"
    ".5 40;33;01\n"
    ".6 40;33;01\n"
    ".7 40;33;01\n"
    ".8 40;33;01\n"
    ".9 40;33;01\n"
    ".man 00;36\n"
    ".1 00;36\n"
    ".2 00;36\n"
    ".3 00;36\n"
    ".4 00;36\n"
    ".5 00;36\n"
    ".6 00;36\n"
    ".7 00;36\n"
    ".8 00;36\n"
    ".9 00;36\n"
    ".mdoc 00;36\n"
    ".1in 00;36\n"
    ".2in 00;36\n"
    ".3in 00;36\n"
    ".4in 00;36\n"
    ".5in 00;36\n"
    ".6in 00;36\n"
    ".7in 00;36\n"
    ".8in 00;36\n"
    ".9in 00;36\n"
    ".n 00;36\n"
    ".l 00;36\n"
    ".roff 00;36\n"
    ".7 00;36\n"
    ".8 00;36\n"
    ".9 00;36\n"
    ".l 00;36\n"
    ".n 00;36\n"
    ".man 00;36\n"
    ".glu 01;35\n"
    ".plt 01;35\n"
    ".gnu 01;35\n"
    ".gnuplot 01;35\n"
    ".plot 01;35\n"
    ".plt 01;35\n"
    ".dat 00;36\n"
    ".out 00;36\n"
    ".diff 00;36\n"
    ".patch 00;36\n"
    ".po 01;35\n"
    ".pot 01;35\n"
    ".mo 01;35\n"
    ".gmo 01;35\n"
    ".am 01;35\n"
    ".in 01;35\n"
    ".inl 01;35\n"
    ".sed 01;35\n"
    ".awk 01;35\n"
    ".f 01;35\n"
    ".f03 01;35\n"
    ".f08 01;35\n"
    ".f90 01;35\n"
    ".f95 01;35\n"
    ".fpp 01;35\n"
    ".for 01;35\n"
    ".fortran 01;35\n"
    ".ttf 01;33\n"
    ".otf 01;33\n"
    ".woff 01;33\n"
    ".woff2 01;33\n"
    ".ttc 01;33\n"
    ".otc 01;33\n"
    ".fnt 01;33\n"
    ".fon 01;33\n"
    ".font 01;33\n"
    ".PFA 01;33\n"
    ".PFB 01;33\n"
    ".pfm 01;33\n"
    ".afm 01;33\n"
    ".sfdir 01;33\n"
    ".eot 01;33\n"
    ".plist 00;36\n"
    ".plist 00;36\n"
    ".plist 00;36\n"
    ".plist 00;36\n"
;

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [FILE]\n", prog);
    printf("Output commands to set the LS_COLORS environment variable.\n");
    printf("\n");
    printf("Determine format of output:\n");
    printf("  -b, --sh, --bourne-shell    output Bourne shell commands\n");
    printf("  -c, --csh, --c-shell        output C shell commands\n");
    printf("  -p, --print-database        output defaults\n");
    printf("      --help                  display this help and exit\n");
    printf("      --version               output version information and exit\n");
    printf("\n");
    printf("With no FILE, read standard input.\n");
}

static void print_version(const char* prog) {
    printf("dircolors (modbox) 1.0\n");
}

static void print_bourne(const char* db) {
    printf("LS_COLORS='");
    const char* p = db;
    bool first = true;
    while (*p) {
        while (*p == '\n' || *p == ' ' || *p == '\t') p++;
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (*p == '\0') break;

        char key[256];
        int ki = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && ki < 254) {
            key[ki++] = *p++;
        }
        key[ki] = '\0';
        if (ki == 0) continue;

        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n') { p++; continue; }

        char val[256];
        int vi = 0;
        while (*p && *p != '\n' && vi < 254) {
            val[vi++] = *p++;
        }
        val[vi] = '\0';
        if (*p == '\n') p++;

        if (!first) printf(":");
        first = false;
        printf("%s=%s", key, val);
    }
    printf("';\n");
    printf("export LS_COLORS\n");
}

static void print_csh(const char* db) {
    printf("setenv LS_COLORS '");
    const char* p = db;
    bool first = true;
    while (*p) {
        while (*p == '\n' || *p == ' ' || *p == '\t') p++;
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (*p == '\0') break;

        char key[256];
        int ki = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && ki < 254) {
            key[ki++] = *p++;
        }
        key[ki] = '\0';
        if (ki == 0) continue;

        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n') { p++; continue; }

        char val[256];
        int vi = 0;
        while (*p && *p != '\n' && vi < 254) {
            val[vi++] = *p++;
        }
        val[vi] = '\0';
        if (*p == '\n') p++;

        if (!first) printf(":");
        first = false;
        printf("%s=%s", key, val);
    }
    printf("';\n");
}

static void print_database() {
    printf("%s", default_database);
}

void dircolors_command(int argc, char** argv) {
    bool bourne = false;
    bool csh = false;
    bool print_db = false;
    const char* filename = nullptr;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0) {
            print_help(argv[0]);
            return;
        }
        if (strcmp(a, "--version") == 0) {
            print_version(argv[0]);
            return;
        }
        if (strcmp(a, "-b") == 0 || strcmp(a, "--sh") == 0 || strcmp(a, "--bourne-shell") == 0) {
            bourne = true;
            continue;
        }
        if (strcmp(a, "-c") == 0 || strcmp(a, "--csh") == 0 || strcmp(a, "--c-shell") == 0) {
            csh = true;
            continue;
        }
        if (strcmp(a, "-p") == 0 || strcmp(a, "--print-database") == 0) {
            print_db = true;
            continue;
        }
        if (a[0] == '-') {
            fprintf(stderr, "dircolors: invalid option '%s'\n", a);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            return;
        }
        filename = a;
    }

    if (print_db) {
        print_database();
        return;
    }

    if (!bourne && !csh) {
        bourne = true;
    }

    const char* db = default_database;
    if (filename) {
        FILE* f = fopen(filename, "r");
        if (!f) {
            fprintf(stderr, "dircolors: %s: No such file or directory\n", filename);
            return;
        }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = (char*)malloc((size_t)sz + 1);
        if (!buf) { fclose(f); return; }
        fread(buf, 1, (size_t)sz, f);
        buf[sz] = '\0';
        fclose(f);
        db = buf;
    }

    if (csh) {
        print_csh(db);
    } else {
        print_bourne(db);
    }
}

REGISTER_COMMAND("dircolors", dircolors_command, "Color setup for ls");
