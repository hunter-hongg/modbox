#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "commands/date.hpp"
#include "commands/command_macros.hpp"

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [+FORMAT]\n", prog);
    printf("Display the current time in the given FORMAT, or set the system date.\n");
    printf("\n");
    printf("  -d, --date=STRING       display time described by STRING\n");
    printf("  -r, --reference=FILE    display the last modification time of FILE\n");
    printf("  -u, --utc, --universal  print or set Coordinated Universal Time (UTC)\n");
    printf("  -R, --rfc-email         output date and time in RFC 5322 format\n");
    printf("  -I[TIMESPEC], --iso-8601[=TIMESPEC]  output date/time in ISO 8601 format\n");
    printf("  -h, --help              display this help and exit\n");
    printf("  --version               output version information and exit\n");
    printf("\n");
    printf("FORMAT controls the output. %%Y year, %%m month, %%d day, %%H hour,\n");
    printf("%%M minute, %%S second, %%F date (%%Y-%%m-%%d), %%T time (%%H:%%M:%%S).\n");
}

static void apply_format(const struct tm* tm, const char* format,
                         bool utc) {
    if (format == NULL) {
        char buf[256];
        const char* fmt = utc ? "%a %b %e %H:%M:%S UTC %Y"
                              : "%a %b %e %H:%M:%S %Z %Y";
        if (strftime(buf, sizeof(buf), fmt, tm) > 0) {
            printf("%s\n", buf);
        }
        return;
    }
    std::string out;
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++;
            char conv[3] = {'%', *p, '\0'};
            char buf[256];
            if (*p == 'N') {
                out += "000000000";
            } else if (*p == 'z' || *p == 'Z') {
                if (strftime(buf, sizeof(buf), conv, tm) > 0) {
                    out += buf;
                }
            } else {
                if (strftime(buf, sizeof(buf), conv, tm) > 0) {
                    out += buf;
                }
            }
        } else {
            out += *p;
        }
    }
    printf("%s\n", out.c_str());
}

static bool parse_date_string(const char* s, time_t* result, bool utc) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_isdst = -1;
    const char* fmt[] = {
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d",
        "%Y/%m/%d %H:%M:%S",
        "%Y/%m/%d",
        "%m/%d/%Y %H:%M:%S",
        "%m/%d/%Y",
        NULL
    };
    for (int i = 0; fmt[i] != NULL; i++) {
        char* end = strptime(s, fmt[i], &tm);
        if (end != NULL && *end == '\0') {
            *result = utc ? timegm(&tm) : mktime(&tm);
            return true;
        }
    }
    return false;
}

void date_command(int argc, char** argv) {
    bool utc = false;
    const char* date_str = NULL;
    const char* ref_file = NULL;
    const char* iso_spec = NULL;
    bool iso = false;
    bool rfc = false;
    const char* plus_format = NULL;
    bool help = false;
    bool version = false;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            help = true;
        } else if (strcmp(a, "--version") == 0) {
            version = true;
        } else if (strcmp(a, "-u") == 0 || strcmp(a, "--utc") == 0
                   || strcmp(a, "--universal") == 0) {
            utc = true;
        } else if (strcmp(a, "-R") == 0 || strcmp(a, "--rfc-email") == 0) {
            rfc = true;
        } else if (strncmp(a, "-d", 2) == 0) {
            if (a[2] == '=') date_str = a + 3;
            else if (strcmp(a, "-d") == 0 && i + 1 < argc) date_str = argv[++i];
            else if (a[1] == 'd') date_str = a + 2;
        } else if (strncmp(a, "--date=", 7) == 0) {
            date_str = a + 7;
        } else if (strncmp(a, "-r", 2) == 0) {
            if (a[2] == '=') ref_file = a + 3;
            else if (strcmp(a, "-r") == 0 && i + 1 < argc) ref_file = argv[++i];
            else if (a[1] == 'r') ref_file = a + 2;
        } else if (strncmp(a, "--reference=", 12) == 0) {
            ref_file = a + 12;
        } else if (strncmp(a, "-I", 2) == 0) {
            iso = true;
            if (a[2] == '=') iso_spec = a + 3;
            else if (a[2] != '\0') iso_spec = a + 2;
        } else if (strncmp(a, "--iso-8601", 11) == 0) {
            iso = true;
            if (strncmp(a, "--iso-8601=", 12) == 0) iso_spec = a + 12;
        } else if (a[0] == '+') {
            plus_format = a + 1;
        } else {
            fprintf(stderr, "date: invalid argument '%s'\n", a);
            return;
        }
    }

    if (help) { print_help(argv[0]); return; }
    if (version) { printf("date (modbox) 1.0\n"); return; }

    time_t t;
    if (ref_file != NULL) {
        struct stat st;
        if (stat(ref_file, &st) != 0) {
            fprintf(stderr, "date: %s: No such file or directory\n", ref_file);
            return;
        }
        t = st.st_mtime;
    } else if (date_str != NULL) {
        if (!parse_date_string(date_str, &t, utc)) {
            fprintf(stderr, "date: invalid date '%s'\n", date_str);
            return;
        }
    } else {
        t = time(NULL);
    }

    struct tm tm_store;
    struct tm* tm = utc ? gmtime_r(&t, &tm_store) : localtime_r(&t, &tm_store);

    if (rfc) {
        char buf[256];
        if (strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", tm) > 0) {
            printf("%s\n", buf);
        }
        return;
    }
    if (iso) {
        const char* fmt = "%Y-%m-%d";
        if (iso_spec != NULL) {
            if (strcmp(iso_spec, "hours") == 0) fmt = "%Y-%m-%dT%H";
            else if (strcmp(iso_spec, "minutes") == 0) fmt = "%Y-%m-%dT%H:%M";
            else if (strcmp(iso_spec, "seconds") == 0) fmt = "%Y-%m-%dT%H:%M:%S";
            else if (strcmp(iso_spec, "ns") == 0) fmt = "%Y-%m-%dT%H:%M:%S";
        }
        char buf[256];
        if (strftime(buf, sizeof(buf), fmt, tm) > 0) {
            printf("%s\n", buf);
        }
        return;
    }

    apply_format(tm, plus_format, utc);
}

REGISTER_COMMAND("date", date_command, "Print or set system date and time");
