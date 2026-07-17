#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands/nohup.hpp"

// EXIT_CANCELED: nohup itself failed (bad option, cannot redirect, etc.).
static const int EXIT_CANCELED = 125;
static const int EXIT_CANNOT_INVOKE = 126;
static const int EXIT_ENOENT = 127;

static void print_help(const char* prog) {
    printf("Usage: %s COMMAND [ARG]...\n", prog);
    printf("  or:  %s OPTION\n", prog);
    printf("Run COMMAND, ignoring hangup signals.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
    printf("\n");
    printf("If standard input is a terminal, redirect it from an unreadable file.\n");
    printf("If standard output is a terminal, append output to 'nohup.out' if possible,\n");
    printf("'$HOME/nohup.out' otherwise.\n");
    printf("If standard error is a terminal, redirect it to standard output.\n");
    printf("To save output to FILE, use '%s COMMAND > FILE'.\n", prog);
}

static void usage_error(const char* prog) {
    fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    exit(EXIT_CANCELED);
}

// Reopen target_fd onto the file at path, mirroring GNU's fd_reopen: the
// descriptor number is preserved so it keeps standing in for stdin/out/err.
static int reopen_fd(int target_fd, const char* path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    if (fd < 0) {
        return -1;
    }
    if (fd != target_fd) {
        if (dup2(fd, target_fd) < 0) {
            int saved = errno;
            close(fd);
            errno = saved;
            return -1;
        }
        close(fd);
    }
    return target_fd;
}

void nohup_command(int argc, char** argv) {
    const char* prog = argv[0];

    // Options are recognized only before the command, and only --help/--version.
    int i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0) {
        i++;
    } else if (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(prog);
            return;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("nohup (modbox) 1.0\n");
            return;
        }
        fprintf(stderr, "%s: unrecognized option '%s'\n", prog, argv[i]);
        usage_error(prog);
    }

    if (i >= argc) {
        fprintf(stderr, "%s: missing operand\n", prog);
        usage_error(prog);
    }

    bool ignoring_input = isatty(STDIN_FILENO);
    bool redirecting_stdout = isatty(STDOUT_FILENO);
    bool stdout_is_a_tty = redirecting_stdout;
    bool redirecting_stderr = isatty(STDERR_FILENO);

    // If standard input is a terminal, replace it with an unreadable file.
    // /dev/null opened write-only ensures any attempt to read evokes an error.
    if (ignoring_input) {
        if (reopen_fd(STDIN_FILENO, "/dev/null", O_WRONLY, 0) < 0) {
            fprintf(stderr, "%s: failed to render standard input unusable: %s\n",
                    prog, strerror(errno));
            exit(EXIT_CANCELED);
        }
        if (!redirecting_stdout) {
            fprintf(stderr, "%s: ignoring input\n", prog);
        }
    }

    // If standard output is a terminal, append it to nohup.out (or $HOME/nohup.out).
    if (redirecting_stdout) {
        std::string in_home;
        std::string file = "nohup.out";
        int flags = O_CREAT | O_WRONLY | O_APPEND;
        mode_t mode = S_IRUSR | S_IWUSR;
        mode_t old_umask = umask(~mode & 0777);

        int fd = reopen_fd(STDOUT_FILENO, file.c_str(), flags, mode);
        if (fd < 0) {
            int saved_errno = errno;
            const char* home = getenv("HOME");
            if (home != nullptr && *home != '\0') {
                in_home = std::string(home) + "/nohup.out";
                fd = reopen_fd(STDOUT_FILENO, in_home.c_str(), flags, mode);
            }
            if (fd < 0) {
                int saved_errno2 = errno;
                umask(old_umask);
                fprintf(stderr, "%s: failed to open '%s': %s\n",
                        prog, file.c_str(), strerror(saved_errno));
                if (!in_home.empty()) {
                    fprintf(stderr, "%s: failed to open '%s': %s\n",
                            prog, in_home.c_str(), strerror(saved_errno2));
                }
                exit(EXIT_CANCELED);
            }
            file = in_home;
        }

        umask(old_umask);
        fprintf(stderr, "%s: %s '%s'\n", prog,
                ignoring_input ? "ignoring input and appending output to"
                               : "appending output to",
                file.c_str());
    }

    // If standard error is a terminal, redirect it to standard output.
    int saved_stderr_fd = STDERR_FILENO;
    if (redirecting_stderr) {
        // Preserve the original stderr so a failed exec can still be reported.
        saved_stderr_fd = fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);

        if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
            if (stdout_is_a_tty) {
                if (saved_stderr_fd >= 0) {
                    dup2(saved_stderr_fd, STDERR_FILENO);
                }
                fprintf(stderr, "%s: failed to redirect standard error: %s\n",
                        prog, strerror(errno));
            }
            exit(EXIT_CANCELED);
        }
    }

    signal(SIGHUP, SIG_IGN);

    execvp(argv[i], &argv[i]);

    int code = (errno == ENOENT) ? EXIT_ENOENT : EXIT_CANNOT_INVOKE;
    int saved_errno = errno;
    if (redirecting_stderr && saved_stderr_fd >= 0) {
        dup2(saved_stderr_fd, STDERR_FILENO);
    }
    fprintf(stderr, "%s: failed to run command '%s': %s\n",
            prog, argv[i], strerror(saved_errno));
    exit(code);
}