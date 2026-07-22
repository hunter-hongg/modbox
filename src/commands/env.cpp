#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <string>

#include "commands/env.hpp"
#include "commands/command_macros.hpp"

extern char** environ;

static void print_help(const char* prog) {
    printf("Usage: %s [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]\n", prog);
    printf("Set each NAME to VALUE in the environment and run COMMAND.\n");
    printf("\n");
    printf("  -i, --ignore-environment  start with an empty environment\n");
    printf("  -0, --null           end each output line with NUL, not newline\n");
    printf("  -u, --unset=NAME     remove variable from the environment\n");
    printf("  -C, --chdir=DIR      change working directory to DIR\n");
    printf("  -h, --help     display this help and exit\n");
    printf("  -V, --version  output version information and exit\n");
}

static int run_command(std::vector<char*>& argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("env: fork");
        return 127;
    }
    if (pid == 0) {
        execvp(argv[0], argv.data());
        fprintf(stderr, "env: '%s': %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        perror("env: waitpid");
        return 127;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
}

void env_command(int argc, char** argv) {
    const char* prog = argv[0];
    bool ignore_env = false;
    bool null_out = false;
    bool split_dash = false;
    const char* chdir_dir = nullptr;
    std::vector<std::string> unsets;
    std::vector<std::string> assignments;
    std::vector<std::string> command;

    int i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if (split_dash) {
            command.push_back(a);
            continue;
        }
        if (strcmp(a, "-i") == 0 || strcmp(a, "--ignore-environment") == 0) {
            ignore_env = true;
        } else if (strcmp(a, "-0") == 0 || strcmp(a, "--null") == 0) {
            null_out = true;
        } else if (strncmp(a, "--unset=", 8) == 0) {
            unsets.push_back(a + 8);
        } else if (strcmp(a, "-u") == 0) {
            if (i + 1 < argc) unsets.push_back(argv[++i]);
            else { fprintf(stderr, "env: option '-u' requires an argument\n"); return; }
        } else if (strncmp(a, "-u", 2) == 0) {
            unsets.push_back(a + 2);
        } else if (strncmp(a, "--chdir=", 8) == 0) {
            chdir_dir = a + 8;
        } else if (strcmp(a, "-C") == 0) {
            if (i + 1 < argc) chdir_dir = argv[++i];
            else { fprintf(stderr, "env: option '-C' requires an argument\n"); return; }
        } else if (strncmp(a, "-C", 2) == 0) {
            chdir_dir = a + 2;
        } else if (strcmp(a, "--help") == 0) {
            print_help(prog);
            return;
        } else if (strcmp(a, "-h") == 0) {
            print_help(prog);
            return;
        } else if (strcmp(a, "--version") == 0) {
            printf("env (modbox) 1.0\n");
            return;
        } else if (strcmp(a, "-V") == 0) {
            printf("env (modbox) 1.0\n");
            return;
        } else if (strcmp(a, "-") == 0) {
            ignore_env = true;
            split_dash = true;
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "env: invalid option -- '%s'\n", a);
            return;
        } else if (strchr(a, '=') != nullptr) {
            assignments.push_back(a);
        } else {
            command.push_back(a);
            i++;
            break;
        }
    }
    for (; i < argc; i++) {
        command.push_back(argv[i]);
    }

    if (ignore_env) {
        environ = nullptr;
    }
    for (const auto& u : unsets) {
        unsetenv(u.c_str());
    }

    for (const auto& a : assignments) {
        std::string s = a;
        size_t eq = s.find('=');
        setenv(s.substr(0, eq).c_str(), s.c_str() + eq + 1, 1);
    }

    if (chdir_dir != nullptr) {
        if (chdir(chdir_dir) != 0) {
            fprintf(stderr, "env: cannot change directory to '%s': %s\n",
                    chdir_dir, strerror(errno));
            return;
        }
    }

    if (command.empty()) {
        for (char** e = environ; *e != nullptr; e++) {
            fputs(*e, stdout);
            fputc(null_out ? '\0' : '\n', stdout);
        }
        return;
    }

    std::vector<char*> argv_exec;
    for (const auto& c : command) argv_exec.push_back(const_cast<char*>(c.c_str()));
    argv_exec.push_back(nullptr);

    int rc = run_command(argv_exec);
    if (rc != 0) exit(rc);
}

REGISTER_COMMAND("env", env_command, "Print environment variables");
