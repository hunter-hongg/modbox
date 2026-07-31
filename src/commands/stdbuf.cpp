#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <argtable3.h>
#include "commands/stdbuf.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

enum class BufferingMode {
    UNCHANGED,
    LINE_BUFFERED,
    UNBUFFERED,
    BLOCK_BUFFERED
};

struct StreamConfig {
    BufferingMode mode = BufferingMode::UNCHANGED;
    size_t block_size = 0;
};

static bool parse_mode(const char* s, StreamConfig& cfg) {
    if (s == nullptr || s[0] == '\0') return false;

    if (s[0] == 'L') {
        cfg.mode = BufferingMode::LINE_BUFFERED;
        return true;
    }
    if (s[0] == '0') {
        cfg.mode = BufferingMode::UNBUFFERED;
        return true;
    }
    char* endptr = nullptr;
    long val = strtol(s, &endptr, 10);
    if (endptr != s && *endptr == '\0' && val > 0) {
        cfg.mode = BufferingMode::BLOCK_BUFFERED;
        cfg.block_size = (size_t)val;
        return true;
    }
    return false;
}

static void apply_buffering(FILE* stream, const StreamConfig& cfg) {
    if (cfg.mode == BufferingMode::UNCHANGED) return;

    if (cfg.mode == BufferingMode::LINE_BUFFERED) {
        setvbuf(stream, nullptr, _IOLBF, BUFSIZ);
    } else if (cfg.mode == BufferingMode::UNBUFFERED) {
        setvbuf(stream, nullptr, _IONBF, 0);
    } else if (cfg.mode == BufferingMode::BLOCK_BUFFERED) {
        char* buf = (char*)malloc(cfg.block_size);
        if (buf) {
            setvbuf(stream, buf, _IOFBF, cfg.block_size);
        }
    }
}

static bool is_long_opt(const char* a, const char* prefix) {
    return strncmp(a, prefix, strlen(prefix)) == 0;
}

int stdbuf_command(int argc, char** argv) {
    StreamConfig stdin_cfg, stdout_cfg, stderr_cfg;
    int cmd_start = -1;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf("Usage: %s [OPTION]... COMMAND [ARG]...\n", argv[0]);
            printf("Run COMMAND with adjusted buffering for its standard streams.\n");
            printf("\n");
            printf("  -i, --input=MODE    adjust stdin buffering\n");
            printf("  -o, --output=MODE   adjust stdout buffering\n");
            printf("  -e, --error=MODE    adjust stderr buffering\n");
            printf("\n");
            printf("MODES:\n");
            printf("  L     line buffered\n");
            printf("  0     unbuffered\n");
            printf("  N     block buffered with N bytes\n");
            printf("\n");
            printf("  -h, --help          display this help and exit\n");
            printf("      --version       output version information and exit\n");
            return 0;
        }

        if (strcmp(a, "--version") == 0) {
            print_version("stdbuf");
            return 0;
        }

        if (strncmp(a, "--input=", 8) == 0) {
            if (!parse_mode(a + 8, stdin_cfg)) {
                fprintf(stderr, "stdbuf: invalid input mode '%s'\n", a + 8);
                return 0;
            }
        } else if (strncmp(a, "--output=", 9) == 0) {
            if (!parse_mode(a + 9, stdout_cfg)) {
                fprintf(stderr, "stdbuf: invalid output mode '%s'\n", a + 9);
                return 0;
            }
        } else if (strncmp(a, "--error=", 8) == 0) {
            if (!parse_mode(a + 8, stderr_cfg)) {
                fprintf(stderr, "stdbuf: invalid error mode '%s'\n", a + 8);
                return 0;
            }
        } else if (strcmp(a, "-i") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "stdbuf: option '-i' requires an argument\n");
                return 0;
            }
            if (!parse_mode(argv[++i], stdin_cfg)) {
                fprintf(stderr, "stdbuf: invalid input mode '%s'\n", argv[i]);
                return 0;
            }
        } else if (strcmp(a, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "stdbuf: option '-o' requires an argument\n");
                return 0;
            }
            if (!parse_mode(argv[++i], stdout_cfg)) {
                fprintf(stderr, "stdbuf: invalid output mode '%s'\n", argv[i]);
                return 0;
            }
        } else if (strcmp(a, "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "stdbuf: option '-e' requires an argument\n");
                return 0;
            }
            if (!parse_mode(argv[++i], stderr_cfg)) {
                fprintf(stderr, "stdbuf: invalid error mode '%s'\n", argv[i]);
                return 0;
            }
        } else if (a[0] == '-') {
            fprintf(stderr, "stdbuf: invalid option -- '%s'\n", a + 1);
            return 0;
        } else {
            cmd_start = i;
            break;
        }
    }

    if (cmd_start < 0) {
        if (stdin_cfg.mode == BufferingMode::UNCHANGED &&
            stdout_cfg.mode == BufferingMode::UNCHANGED &&
            stderr_cfg.mode == BufferingMode::UNCHANGED) {
            fprintf(stderr, "stdbuf: no mode specified\n");
        } else {
            fprintf(stderr, "stdbuf: missing command\n");
        }
        return 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        apply_buffering(stdin, stdin_cfg);
        apply_buffering(stdout, stdout_cfg);
        apply_buffering(stderr, stderr_cfg);

        execvp(argv[cmd_start], &argv[cmd_start]);
        fprintf(stderr, "stdbuf: failed to execute '%s': %s\n", argv[cmd_start], strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
        if (WIFEXITED(status)) {
            exit(WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            exit(128 + WTERMSIG(status));
        }
        exit(1);
    } else {
        fprintf(stderr, "stdbuf: fork failed: %s\n", strerror(errno));
        return 0;
    }
    return 0;
}

REGISTER_COMMAND("stdbuf", stdbuf_command, "Run COMMAND with adjusted buffering");
