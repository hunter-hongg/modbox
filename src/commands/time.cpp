#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "commands/time.hpp"
#include "commands/command_macros.hpp"

namespace {

const int EXIT_CANCELED = 125;      // time itself failed
const int EXIT_CANNOT_INVOKE = 126;  // command found but could not be run
const int EXIT_ENOENT = 127;         // command not found

struct TimeInfo {
    struct rusage ru;
    double elapsed;   // wall-clock seconds
    int exit_status;  // exit code, or 128+signal if killed by signal
    int signalled;    // signal number if killed by a signal, else 0
    std::string command;
};

void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... COMMAND [ARG]...\n", prog);
    printf("Run COMMAND, then display timing statistics on standard error.\n");
    printf("\n");
    printf("  -p, --portable       use the POSIX portable output format\n");
    printf("  -v, --verbose        print a detailed report of resource usage\n");
    printf("  -f FORMAT, --format=FORMAT\n");
    printf("                       use FORMAT as the output format string\n");
    printf("  -o FILE, --output=FILE\n");
    printf("                       write the statistics to FILE instead of stderr\n");
    printf("  -a, --append         with -o FILE, append instead of overwriting\n");
    printf("      --help           display this help and exit\n");
    printf("      --version        output version information and exit\n");
    printf("\n");
    printf("FORMAT interprets these '%%' specifiers:\n");
    printf("  %%U user CPU seconds   %%S system CPU seconds   %%E elapsed (h:mm:ss)\n");
    printf("  %%e elapsed seconds    %%P percent CPU          %%M max resident KB\n");
    printf("  %%I fs inputs          %%O fs outputs           %%F major faults\n");
    printf("  %%R minor faults       %%W swaps                %%c involuntary ctx switches\n");
    printf("  %%w voluntary ctx switches   %%C command        %%x exit status   %%%% literal %%\n");
}

double tv_to_sec(const struct timeval* tv) {
    return static_cast<double>(tv->tv_sec) + static_cast<double>(tv->tv_usec) / 1000000.0;
}

// Append the GNU-style elapsed time (%E): "h:mm:ss" when >= 1 hour, else
// "m:ss.cc" with hundredths of a second.
void append_elapsed(std::string* out, double secs) {
    if (secs < 0) secs = 0;
    long total = static_cast<long>(secs);
    int hours = static_cast<int>(total / 3600);
    int minutes = static_cast<int>((total % 3600) / 60);
    char buf[64];
    if (hours > 0) {
        int isecs = static_cast<int>(total % 60);
        snprintf(buf, sizeof(buf), "%d:%02d:%02d", hours, minutes, isecs);
    } else {
        double fsecs = secs - static_cast<double>(hours * 3600 + minutes * 60);
        snprintf(buf, sizeof(buf), "%d:%05.2f", minutes, fsecs);
    }
    out->append(buf);
}

void append_double(std::string* out, double v, int prec) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", prec, v);
    out->append(buf);
}

void append_long(std::string* out, long v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%ld", v);
    out->append(buf);
}

// Expand a GNU time format string into a summary line.
std::string expand_format(const char* fmt, const TimeInfo& info) {
    double user = tv_to_sec(&info.ru.ru_utime);
    double sys = tv_to_sec(&info.ru.ru_stime);
    double cpu = user + sys;
    int pct = (info.elapsed > 0.0) ? static_cast<int>((cpu / info.elapsed) * 100.0 + 0.5) : 0;

    std::string out;
    for (const char* p = fmt; *p != '\0'; ++p) {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case '\\': out.push_back('\\'); break;
                case '\0': out.push_back('\\'); --p; break;
                default: out.push_back('\\'); out.push_back(*p); break;
            }
            continue;
        }
        if (*p != '%') {
            out.push_back(*p);
            continue;
        }
        ++p;
        switch (*p) {
            case 'U': append_double(&out, user, 2); break;
            case 'S': append_double(&out, sys, 2); break;
            case 'e': append_double(&out, info.elapsed, 2); break;
            case 'E': append_elapsed(&out, info.elapsed); break;
            case 'P': append_long(&out, pct); out.push_back('%'); break;
            case 'M': append_long(&out, info.ru.ru_maxrss); break;
            case 'K': case 'D': case 'X': case 'p': case 't': case 'Z':
                out.push_back('0');
                break;
            case 'I': append_long(&out, info.ru.ru_inblock); break;
            case 'O': append_long(&out, info.ru.ru_oublock); break;
            case 'F': append_long(&out, info.ru.ru_majflt); break;
            case 'R': append_long(&out, info.ru.ru_minflt); break;
            case 'W': append_long(&out, info.ru.ru_nswap); break;
            case 'c': append_long(&out, info.ru.ru_nivcsw); break;
            case 'w': append_long(&out, info.ru.ru_nvcsw); break;
            case 'r': append_long(&out, info.ru.ru_msgrcv); break;
            case 's': append_long(&out, info.ru.ru_msgsnd); break;
            case 'k': append_long(&out, info.ru.ru_nsignals); break;
            case 'C': out.append(info.command); break;
            case 'x': append_long(&out, info.exit_status); break;
            case '%': out.push_back('%'); break;
            case '\0': out.push_back('%'); --p; break;
            default: out.push_back('%'); out.push_back(*p); break;
        }
    }
    return out;
}

void print_verbose(FILE* fp, const TimeInfo& info) {
    double user = tv_to_sec(&info.ru.ru_utime);
    double sys = tv_to_sec(&info.ru.ru_stime);
    double cpu = user + sys;
    int pct = (info.elapsed > 0.0) ? static_cast<int>((cpu / info.elapsed) * 100.0 + 0.5) : 0;

    fprintf(fp, "\tCommand being timed: \"%s\"\n", info.command.c_str());
    fprintf(fp, "\tUser time (seconds): %.2f\n", user);
    fprintf(fp, "\tSystem time (seconds): %.2f\n", sys);
    fprintf(fp, "\tPercent of CPU this job got: %d%%\n", pct);
    std::string elapsed;
    append_elapsed(&elapsed, info.elapsed);
    fprintf(fp, "\tElapsed (wall clock) time (h:mm:ss or m:ss): %s\n", elapsed.c_str());
    fprintf(fp, "\tMaximum resident set size (kbytes): %ld\n", info.ru.ru_maxrss);
    fprintf(fp, "\tMajor (requiring I/O) page faults: %ld\n", info.ru.ru_majflt);
    fprintf(fp, "\tMinor (reclaiming a frame) page faults: %ld\n", info.ru.ru_minflt);
    fprintf(fp, "\tVoluntary context switches: %ld\n", info.ru.ru_nvcsw);
    fprintf(fp, "\tInvoluntary context switches: %ld\n", info.ru.ru_nivcsw);
    fprintf(fp, "\tSwaps: %ld\n", info.ru.ru_nswap);
    fprintf(fp, "\tFile system inputs: %ld\n", info.ru.ru_inblock);
    fprintf(fp, "\tFile system outputs: %ld\n", info.ru.ru_oublock);
    fprintf(fp, "\tSocket messages sent: %ld\n", info.ru.ru_msgsnd);
    fprintf(fp, "\tSocket messages received: %ld\n", info.ru.ru_msgrcv);
    fprintf(fp, "\tSignals delivered: %ld\n", info.ru.ru_nsignals);
    fprintf(fp, "\tExit status: %d\n", info.exit_status);
}

}  // namespace

void time_command(int argc, char** argv) {
    const char* prog = argv[0];

    bool portable = false;
    bool verbose = false;
    bool append = false;
    const char* format = nullptr;
    const char* output_file = nullptr;

    int i = 1;
    while (i < argc) {
        const char* s = argv[i];

        if (strcmp(s, "--") == 0) {
            ++i;
            break;
        }
        if (strcmp(s, "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(s, "--version") == 0) {
            printf("time (modbox) 1.0\n");
            return;
        }
        if (strcmp(s, "-p") == 0 || strcmp(s, "--portable") == 0) {
            portable = true;
            ++i;
            continue;
        }
        if (strcmp(s, "-v") == 0 || strcmp(s, "--verbose") == 0) {
            verbose = true;
            ++i;
            continue;
        }
        if (strcmp(s, "-a") == 0 || strcmp(s, "--append") == 0) {
            append = true;
            ++i;
            continue;
        }
        if (strcmp(s, "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option requires an argument -- 'f'\n", prog);
                exit(EXIT_CANCELED);
            }
            format = argv[i + 1];
            i += 2;
            continue;
        }
        if (strncmp(s, "--format=", 9) == 0) {
            format = s + 9;
            ++i;
            continue;
        }
        if (strcmp(s, "--format") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option '--format' requires an argument\n", prog);
                exit(EXIT_CANCELED);
            }
            format = argv[i + 1];
            i += 2;
            continue;
        }
        if (strcmp(s, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option requires an argument -- 'o'\n", prog);
                exit(EXIT_CANCELED);
            }
            output_file = argv[i + 1];
            i += 2;
            continue;
        }
        if (strncmp(s, "--output=", 9) == 0) {
            output_file = s + 9;
            ++i;
            continue;
        }
        if (strcmp(s, "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option '--output' requires an argument\n", prog);
                exit(EXIT_CANCELED);
            }
            output_file = argv[i + 1];
            i += 2;
            continue;
        }
        if (s[0] == '-' && s[1] != '\0') {
            fprintf(stderr, "%s: unrecognized option '%s'\n", prog, s);
            fprintf(stderr, "Try '%s --help' for more information.\n", prog);
            exit(EXIT_CANCELED);
        }

        // First non-option argument: the command begins here.
        break;
    }

    if (i >= argc) {
        fprintf(stderr, "%s: missing program to run\n", prog);
        fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        exit(EXIT_CANCELED);
    }

    // Build the command string for %C / verbose output.
    std::string command;
    for (int j = i; j < argc; ++j) {
        if (j > i) command.push_back(' ');
        command.append(argv[j]);
    }

    struct timeval start;
    struct timeval finish;
    gettimeofday(&start, nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "%s: cannot fork: %s\n", prog, strerror(errno));
        exit(EXIT_CANCELED);
    }

    if (pid == 0) {
        execvp(argv[i], &argv[i]);
        int code = (errno == ENOENT) ? EXIT_ENOENT : EXIT_CANNOT_INVOKE;
        fprintf(stderr, "%s: cannot run %s: %s\n", prog, argv[i], strerror(errno));
        _exit(code);
    }

    int status = 0;
    struct rusage ru;
    memset(&ru, 0, sizeof(ru));
    while (wait4(pid, &status, 0, &ru) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "%s: wait failed: %s\n", prog, strerror(errno));
            exit(EXIT_CANCELED);
        }
    }

    gettimeofday(&finish, nullptr);

    TimeInfo info;
    info.ru = ru;
    info.elapsed = tv_to_sec(&finish) - tv_to_sec(&start);
    if (info.elapsed < 0) info.elapsed = 0;
    info.command = command;
    info.signalled = 0;
    if (WIFEXITED(status)) {
        info.exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        info.signalled = WTERMSIG(status);
        info.exit_status = 128 + info.signalled;
    } else {
        info.exit_status = 0;
    }

    // Select the destination stream for the report.
    FILE* fp = stderr;
    if (output_file != nullptr) {
        fp = fopen(output_file, append ? "a" : "w");
        if (fp == nullptr) {
            fprintf(stderr, "%s: cannot open %s: %s\n", prog, output_file, strerror(errno));
            exit(EXIT_CANCELED);
        }
    }

    if (verbose) {
        print_verbose(fp, info);
    } else if (format != nullptr) {
        std::string line = expand_format(format, info);
        fputs(line.c_str(), fp);
        fputc('\n', fp);
    } else if (portable) {
        double user = tv_to_sec(&info.ru.ru_utime);
        double sys = tv_to_sec(&info.ru.ru_stime);
        fprintf(fp, "real %.2f\nuser %.2f\nsys %.2f\n", info.elapsed, user, sys);
    } else {
        const char* def =
            "%Uuser %Ssystem %Eelapsed %PCPU (%Xavgtext+%Davgdata %Mmaxresident)k\n"
            "%Iinputs+%Ooutputs (%Fmajor+%Rminor)pagefaults %Wswaps";
        std::string line = expand_format(def, info);
        fputs(line.c_str(), fp);
        fputc('\n', fp);
    }

    if (output_file != nullptr) {
        fclose(fp);
    }

    exit(info.exit_status);
}

REGISTER_COMMAND("time", time_command, "Run and report timing statistics");
