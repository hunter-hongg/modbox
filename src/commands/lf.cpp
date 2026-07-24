#include <cstdlib>
#include <cstring>

#include "commands/lf.hpp"
#include "commands/ls.hpp"
#include "commands/command_macros.hpp"

void lf_command(int argc, char** argv) {
    if (argc >= 2 && strcmp(argv[1], "init") == 0) {
        const char* shell = (argc >= 3) ? argv[2] : "";
        if (shell[0] == '\0') {
            fprintf(stderr, "lf: please specify a shell: bash, zsh, fish\n");
            return;
        }
        if (strcmp(shell, "bash") == 0 || strcmp(shell, "zsh") == 0) {
            printf("lf() {\n");
            printf("  local __cwd_file=\"${HOME}/.cache/lf/cwd\"\n");
            printf("  command modbox lf --tui \"$@\"\n");
            printf("  if [ -f \"$__cwd_file\" ] && [ -r \"$__cwd_file\" ]; then\n");
            printf("    local __lf_last_dir\n");
            printf("    __lf_last_dir=$(<\"$__cwd_file\")\n");
            printf("    if [ -n \"$__lf_last_dir\" ] && [ \"$__lf_last_dir\" != \"$(pwd)\" ]; then\n");
            printf("      cd \"$__lf_last_dir\"\n");
            printf("    fi\n");
            printf("  fi\n");
            printf("}\n");
        } else if (strcmp(shell, "fish") == 0) {
            printf("function lf\n");
            printf("  set -l __cwd_file \"$HOME/.cache/lf/cwd\"\n");
            printf("  command modbox lf --tui $argv\n");
            printf("  if test -f \"$__cwd_file\"\n");
            printf("    set -l __lf_last_dir (cat \"$__cwd_file\")\n");
            printf("    if test -n \"$__lf_last_dir\" && test \"$__lf_last_dir\" != (pwd)\n");
            printf("      cd \"$__lf_last_dir\"\n");
            printf("    end\n");
            printf("  end\n");
            printf("end\n");
        } else {
            fprintf(stderr, "lf: unsupported shell '%s'. Supported: bash, zsh, fish\n", shell);
        }
        return;
    }

    int new_argc = argc + 1;
    char** new_argv = (char**)malloc((size_t)(new_argc + 1) * sizeof(char*));

    new_argv[0] = argv[0];
    new_argv[1] = "--tui";

    for (int i = 1; i < argc; i++) {
        new_argv[i + 1] = argv[i];
    }
    new_argv[new_argc] = nullptr;

    ls_command(new_argc, new_argv);

    free(new_argv);
}

REGISTER_COMMAND("lf", lf_command, "Interactive file browser (alias for ls --tui)");
