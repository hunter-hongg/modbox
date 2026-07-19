#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "commands/echo.hpp"

void echo_command(int argc, char** argv) {
    bool no_newline = false;
    bool interpret = false;
    std::vector<const char*> args;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (i == 1) {
            if (strcmp(a, "-n") == 0) {
                no_newline = true;
                continue;
            }
            if (strcmp(a, "-e") == 0) {
                interpret = true;
                continue;
            }
            if (strcmp(a, "-E") == 0) {
                interpret = false;
                continue;
            }
            if (strcmp(a, "--help") == 0) {
                printf("Usage: %s [SHORT-OPTION]... [STRING]...\n", argv[0]);
                printf("Echo the STRING(s) to standard output.\n");
                printf("\n");
                printf("  -n             do not output the trailing newline\n");
                printf("  -e             enable interpretation of backslash escapes\n");
                printf("  -E             disable interpretation of backslash escapes\n");
                return;
            }
            if (strcmp(a, "--version") == 0) {
                printf("echo (modbox) 1.0\n");
                return;
            }
        }
        args.push_back(a);
    }

    std::string out;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) out += ' ';
        if (interpret) {
            const char* s = args[i];
            for (size_t j = 0; s[j] != '\0'; j++) {
                if (s[j] == '\\' && s[j + 1] != '\0') {
                    j++;
                    switch (s[j]) {
                        case 'a': out += '\a'; break;
                        case 'b': out += '\b'; break;
                        case 'c': no_newline = true; fputs(out.c_str(), stdout); return; /* stop */
                        case 'e': out += '\x1b'; break;
                        case 'f': out += '\f'; break;
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;
                        case 'v': out += '\v'; break;
                        case '\\': out += '\\'; break;
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7': {
                            int val = s[j] - '0';
                            int k = 1;
                            while (s[j + 1] != '\0' && s[j + 1] >= '0'
                                   && s[j + 1] <= '7' && k < 3) {
                                j++;
                                val = val * 8 + (s[j] - '0');
                                k++;
                            }
                            out += (char)val;
                            break;
                        }
                        default:
                            out += '\\';
                            out += s[j];
                            break;
                    }
                } else {
                    out += s[j];
                }
            }
        } else {
            out += args[i];
        }
    }

    fputs(out.c_str(), stdout);
    if (!no_newline) fputc('\n', stdout);
}
